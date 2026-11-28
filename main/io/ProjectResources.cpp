// ============================================================================
//  ProjectResources.cpp — la BIBLIOTECA del proyecto (ver el .h): los proveedores
//  de este modulo (mallas, materiales, texturas, animsets y los archivos: sonidos,
//  scripts, fuentes, videos), el arbol UNICO de carpetas cosmeticas con su undo,
//  el recurso activo, los datos por tipo, dentro/fuera del .w3d y el guardado.
// ============================================================================
#include "io/ProjectResources.h"
#include "objects/ReflectionProbe.h"     // la sonda de reflejo: usuaria de su imagen 360
#include "render/RenderOptions.h"        // g_renderBgTexs: el fondo es usuario de sus texturas
#include "io/Streaming.h"            // los pedidos del streaming son usuarios de una textura
#include "W3dRoots.h"                  // los recursos se cuentan en TODAS las raices (escenas/prefabs)
#include "io/W3dContainer.h"
#include "io/JsonW3d.h"
#include "io/SaveAnimSets.h"           // usuarios / renombrar / carpeta de los animsets
#include "io/ProjectMeshes.h"          // TODA malla es un recurso: se asegura antes de listar
#include "WhiskUI/io/UI2DFormat.h"             // la carpeta del .w3d abierto (base de las rutas "ext:")
#include "io/ResourceData.h"           // los datos de imagenes y sonidos (Properties)
#include "w3dTexture.h"                // TextureMemoryOf (lo que ocupa una textura en la GPU)
#include "io/Thumbnails.h"             // las miniaturas de la cuadricula se sueltan al cerrar el proyecto
#include "base/W3dInteractionState.h"  // InteractionMode (borrar objetos solo en Object Mode)
#include "w3dGraphics.h"               // MipmapsGlobal (la memoria de una textura)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Armature.h"
#include "objects/Particles.h"
#include "WhiskUI/objects/Image2D.h"
#include "WhiskUI/objects/Slice9.h"
#include "WhiskUI/objects/Button2D.h"
#include "WhiskUI/objects/Text2D.h"            // la FUENTE BITMAP de un texto 2D es un usuario de textura
#include "WhiskUI/objects/Video2D.h"           // el archivo de un video 2D
#include "objects/Sound.h"             // el archivo de un objeto Sonido
#include "animation/W3dAnimSet.h"
#include "animation/SkeletalAnimation.h" // W3D_RETARGET_*: el retarget por defecto de un clip de jerarquia
#include "animation/Flipbook.h"        // SceneFlipbooks: el atlas de un flipbook es un usuario de textura
#include "animation/Animation.h"       // w3dGetTicks: lo leido de un archivo se recuerda un rato
#include "io/W3dResources.h"
#include "io/W3dZip.h"                 // el contenedor VIEJO del guardado (la entrada se conserva?)
#include "script/W3dScript.h"          // W3dScriptData (las rutas de los .lua de cada objeto)
#include "importers/import_obj.h"      // PendingTextureOf: la textura base todavia en la cola
#include "io/EditedTexture.h"          // la pintura de una textura purgada en caliente suelta sus pixeles
#include "base/W3dNames.h"
#include "undo/Undo.h"
#include "W3dNotice.h"
#include "config/W3dLang.h"            // T(): el aviso de los scripts al renombrar una textura
#include "WhiskUI/draw/icons.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <algorithm>
#include <map>
#include <set>
#include <cstdio>
#include <cstring>

// LO NO GUARDADO (io/ProjectChanges.h): un paso de undo de la BIBLIOTECA (una carpeta, el nombre de un recurso,
// un material nuevo) no es una edicion de la RAIZ activa: mientras vive este guard el undo no la marca
extern int g_undoNoRoot;   // undo/Undo.cpp
struct NoRoot { NoRoot() { g_undoNoRoot++; } ~NoRoot() { g_undoNoRoot--; } };

// ============================================================================
//  LOS TIPOS
// ============================================================================
static const char* const kKeys[W3D_VIEWS] = {
    "scene", "meshes", "materials", "textures", "animations", "prefabs", "scenes", "libs",
    "sounds", "scripts", "fonts", "videos", "shaders", "missing"
};
static const char* const kTitles[W3D_VIEWS] = {
    "Scene", "3D Meshes", "Materials", "Textures", "Animations", "Prefabs", "Scenes", "Libraries",
    "Sounds", "Scripts", "Fonts", "Videos", "Shaders", "Missing files"
};
static const char* const kSingular[W3D_VIEWS] = {
    "Scene", "3D Mesh", "Material", "Texture", "Animation", "Prefab", "Scene", "Library",
    "Sound", "Script", "Font", "Video", "Shader", "Missing file"
};
const char* W3dViewKey(int v)    { return (v >= 0 && v < W3D_VIEWS) ? kKeys[v] : ""; }
const char* W3dTitleView(int v)   { return (v >= 0 && v < W3D_VIEWS) ? kTitles[v] : ""; }
const char* W3dViewSingular(int v) { return (v >= 0 && v < W3D_VIEWS) ? kSingular[v] : ""; }
int W3dViewByKey(const std::string& c) {
    for (int v = 0; v < W3D_VIEWS; v++) if (c == kKeys[v]) return v;
    return -1;
}
int W3dIconView(int v) {
    switch (v) {
        case W3D_VIEW_SCENE:      return (int)IconType::archive;
        case W3D_VIEW_MESHES:      return (int)IconType::mesh;
        case W3D_VIEW_MATERIALS:  return (int)IconType::material;
        case W3D_VIEW_TEXTURES:    return (int)IconType::texture;
        case W3D_VIEW_ANIMATIONS: return (int)IconType::animation;
        case W3D_VIEW_PREFABS:     return (int)IconType::prefab;
        case W3D_VIEW_SCENES:     return (int)IconType::camera;   // (el de una raiz ESCENA)
        case W3D_VIEW_LIBS:   return (int)IconType::lib;
        case W3D_VIEW_SOUNDS:     return (int)IconType::sound;
        case W3D_VIEW_SCRIPTS:     return (int)IconType::script;
        case W3D_VIEW_FONTS:     return (int)IconType::list;    // (el de un texto 2D)
        case W3D_VIEW_VIDEOS:      return (int)IconType::video;
        case W3D_VIEW_SHADERS:     return (int)IconType::script;
        case W3D_VIEW_MISSING:      return (int)IconType::notifError;
    }
    return (int)IconType::archive;
}

std::string W3dResourceProvider::IdRequest(const std::string& id, const std::string& newValue) {
    return W3dNameNormalize(newValue, id.c_str());
}

bool W3dResourceProvider::Info(const std::string& id, W3dResourceItem& out) {
    std::vector<W3dResourceItem> v;
    List(v);
    for (size_t i = 0; i < v.size(); i++) if (v[i].id == id) { out = v[i]; return true; }
    return false;
}
// los datos por defecto: la linea de datos del item
void W3dResourceProvider::Data(const std::string& id, std::vector<W3dResourceData>& out) {
    out.clear();
    W3dResourceItem it;
    if (Info(id, it) && !it.info.empty()) out.push_back(W3dResourceData(T("Info"), it.info));
}

// ============================================================================
//  RECORRIDOS DE LA ESCENA (un pase por listado: contar usuarios es O(objetos))
// ============================================================================
static void CollectObjects(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        out.push_back(h);
        CollectObjects(h, out);
    }
}
// los objetos de TODAS las raices del proyecto: la escena activa y las escenas/prefabs cargados que
// no se estan mirando (W3dRoots.h). Un recurso que solo usa otra escena NO es huerfano.
static void ObjectsScene(std::vector<Object*>& out) {
    out.clear();
    std::vector<Object*> roots;
    W3dLiveRoots(roots);
    for (size_t i = 0; i < roots.size(); i++) CollectObjects(roots[i], out);
}
// 'fn' sobre cada raiz viva (para los recorridos que ya reciben una raiz)
static bool InAnyRoot(bool (*fn)(Object*, const std::string&), const std::string& a) {
    std::vector<Object*> roots;
    W3dLiveRoots(roots);
    for (size_t i = 0; i < roots.size(); i++) if (fn(roots[i], a)) return true;
    return false;
}

// BORRA esos objetos de la escena ACTIVA con su paso de undo (el Delete de siempre: los detacha y el
// Ctrl+Z los devuelve). false = alguno vive en otra escena (no esta en el arbol que se edita) o no se
// puede borrar ahora (fuera de Object Mode).
extern void Remove(bool IncludeCollections);   // objects/ObjectMode.cpp
static bool W3dDeleteObjectsWithUndo(const std::vector<Object*>& us) {
    if (InteractionMode != ObjectMode || !SceneCollection) return false;
    for (size_t i = 0; i < us.size(); i++) if (!us[i] || W3dRootOf(us[i]) != SceneCollection) return false;
    DeselectAll();
    for (size_t i = 0; i < us.size(); i++) { us[i]->select = true; ObjSelects.push_back(us[i]); }
    Remove(false);
    return true;
}

static std::string IntegerText(long n) { char b[32]; sprintf(b, "%ld", n); return std::string(b); }
// "24 vertices", "1 part": el numero y la palabra TRADUCIDA, en singular o plural. Los datos de un
// recurso (W3dResourceItem::info) se arman ya en el idioma de la UI: un texto con numeros adentro no
// esta en la tabla y T() lo devolveria en ingles.
static std::string Count(long n, const char* one, const char* several) {
    return IntegerText(n) + " " + T(n == 1 ? one : several);
}

// el nombre corto de una ruta ("textures/piso.png" -> "piso.png")
static std::string BaseFromPath(const std::string& r) {
    size_t p = r.find_last_of("/\\");
    return (p == std::string::npos) ? r : r.substr(p + 1);
}

// "yes" / "no" en el idioma de la UI
static std::string IfNo(bool b) { return std::string(T(b ? "Yes" : "No")); }

// el TAMANO de un archivo del proyecto: una entrada (la editada en la sesion o la del zip montado) o
// un archivo de disco (una referencia externa). -1 = no esta.
static long FileSize(const std::string& r) {
    if (r.empty()) return -1;
    std::vector<unsigned char> ed;
    if (W3dContainerReadEdited(r, ed)) return (long)ed.size();
    W3dZipReader* z = W3dContainerReader();
    if (z && z->Exists(r)) return (long)z->Size(r);
    FILE* f = fopen(r.c_str(), "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fclose(f);
    return n;
}
// los bytes de un archivo del proyecto (entrada o disco). false = no se pudo leer.
static bool BytesFile(const std::string& r, std::vector<unsigned char>& out) {
    out.clear();
    return !r.empty() && w3dFileSystem::ReadFileBytes(r, out) && !out.empty();
}
// LO QUE DICE EL ARCHIVO de una textura o un sonido (sus encabezados) y cuantos bytes tiene. La tarjeta del
// recurso en Properties pide los datos en CADA cuadro que se dibuja: leer el archivo entero (inflarlo del
// .w3d) y decodificar la imagen para saber que alfa tiene, cuadro por cuadro, trababa la UI con una textura
// grande o un video. Se recuerda el ULTIMO que se pidio; se vuelve a leer si cambio su tamano, si no se
// sabe su tamano, si la biblioteca se invalido (W3dResourcesViewInvalidate: un rename, adentro/afuera...) o
// cada 2 s (un archivo externo que se reemplazo afuera por otro del mismo tamano).
namespace {
struct InfoFile {
    std::string id;
    int type;                // la vista que lo pidio (imagen o sonido: se decodifica solo lo suyo)
    long size;               // FileSize al leerlo
    long bytes;              // los bytes leidos (0 = no se pudo)
    unsigned read;           // w3dGetTicks al leerlo
    bool isImage, isSound;
    W3dImageData im;
    W3dSoundData sound;
    InfoFile() : type(-1), size(-1), bytes(0), read(0), isImage(false), isSound(false) {}
};
}
static InfoFile gInfoArch;
static const InfoFile& FileInfo(int type, const std::string& id) {
    const long size = FileSize(id);
    const unsigned now = w3dGetTicks();
    if (size >= 0 && gInfoArch.type == type && gInfoArch.size == size && gInfoArch.id == id && now - gInfoArch.read < 2000)
        return gInfoArch;
    gInfoArch = InfoFile();
    gInfoArch.id = id; gInfoArch.type = type; gInfoArch.size = size; gInfoArch.read = now;
    std::vector<unsigned char> b;
    if (BytesFile(id, b)) {   // (sin tamano conocido se lee igual, pero no se recuerda: ver arriba)
        gInfoArch.bytes = (long)b.size();
        if (type == W3D_VIEW_TEXTURES) gInfoArch.isImage = W3dImageInfo(&b[0], b.size(), gInfoArch.im);
        if (type == W3D_VIEW_SOUNDS)  gInfoArch.isSound = W3dSoundInfo(&b[0], b.size(), gInfoArch.sound);
    }
    return gInfoArch;
}

// ============================================================================
//  PROVEEDOR: MALLAS 3D (el registro de objects/MeshResource.h)
// ============================================================================
static void UsersMeshes(std::map<const MeshResource*, int>& count, std::vector<Object*>* all) {
    std::vector<Object*> objs;
    ObjectsScene(objs);
    for (size_t i = 0; i < objs.size(); i++) {
        if (objs[i]->getType() != ObjectType::mesh) continue;
        const Mesh* m = (const Mesh*)objs[i];
        if (!m->mesh) continue;
        count[m->mesh]++;
        // un recurso que el usuario BORRO y que un undo le devolvio a un objeto vuelve a ser del proyecto
        if (m->mesh->deleted) m->mesh->deleted = false;
    }
    if (all) all->swap(objs);
}
// el recurso de la biblioteca con ese nombre (sin los BORRADOS, que ya no son del proyecto)
static MeshResource* LiveMesh(const std::string& id) {
    MeshResource* r = W3dMeshResourceByName(id);
    return (r && !r->deleted) ? r : NULL;
}

class MeshProvider : public W3dResourceProvider {
public:
    static void Item(const MeshResource* r, int users, W3dResourceItem& it) {
        it.id = r->name;
        it.name = r->name;
        it.folder = r->folder;
        it.entry  = r->entry;
        it.icon = (int)IconType::mesh;
        it.users = users;
        // los numeros VIVOS: los de un usuario (la geometria que se esta editando todavia no se publico al recurso: el
        // tamanio quedaba en el de la ultima publicacion). Solo cuando se lista (el outliner mostrando la biblioteca)
        int nV = r->vertexSize, nI = r->facesSize;
        const bool hasUser = !r->users.empty() && r->users[0];
        if (hasUser) { nV = r->users[0]->vertexSize; nI = r->users[0]->facesSize; }
        it.loaded = r->Loaded() || hasUser;
        if (it.loaded && nV > 0)
            it.info = Count(nV, "vertex", "vertices") + ", " + Count(nI / 3, "triangle", "triangles") +
                      ", " + Count((long)r->parts.size(), "part", "parts");
        else it.info = T("not loaded");   // (un huerfano que nadie pidio: sus arrays no estan en memoria)
        // lo que ocupan sus arrays de render: posicion 12 + normal 3 + uv 8 + color 4 bytes por vertice, 2 por indice
        if (it.loaded) it.bytes = (long)nV * 27L + (long)nI * 2L;
    }
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        std::map<const MeshResource*, int> count;
        UsersMeshes(count, NULL);
        const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
        for (size_t i = 0; i < reg.size(); i++) {
            if (reg[i]->deleted) continue;   // (el usuario lo borro: vive solo para el undo)
            W3dResourceItem it;
            Item(reg[i], count[reg[i]], it);
            out.push_back(it);
        }
    }
    bool Info(const std::string& id, W3dResourceItem& out) {
        MeshResource* r = LiveMesh(id);
        if (!r) return false;
        Item(r, W3dMeshResourceUsersInScene(r), out);
        return true;
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        MeshResource* r = LiveMesh(id);
        if (!r) return false;
        r->folder = folder;
        return true;
    }
    bool Duplicate(const std::string& id, std::string* idNew, std::string* reason) {
        MeshResource* r = LiveMesh(id);
        if (!r) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        MeshResource* n = W3dMeshDuplicateResource(r);
        if (!n) { if (reason) *reason = "Its geometry couldn't be read"; return false; }
        if (idNew) *idNew = n->name;
        return true;
    }
    // EN USO: sus objetos se BORRAN (con Ctrl+Z, como el Delete de la escena: una malla no puede quedar
    // sin su malla 3D) y el recurso deja de ser del proyecto; el undo devuelve los objetos con su recurso
    bool KnowsDeleteInUse() const { return true; }
    bool DeleteInUse(const std::string& id, std::string* reason) {
        MeshResource* r = LiveMesh(id);
        if (!r) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        std::vector<Object*> us;
        Users(id, us);
        if (!us.empty() && !W3dDeleteObjectsWithUndo(us)) { if (reason) *reason = "Its objects couldn't be deleted"; return false; }
        if (!W3dMeshResourceDelete(r)) r->deleted = true;   // (lo retiene el undo: se va cuando lo suelte)
        return true;
    }
    void Data(const std::string& id, std::vector<W3dResourceData>& out) {
        out.clear();
        MeshResource* r = LiveMesh(id);
        if (!r) return;
        if (r->Loaded()) {
            out.push_back(W3dResourceData(T("Vertices"), IntegerText(r->vertexSize)));
            out.push_back(W3dResourceData(T("Triangles"), IntegerText(r->facesSize / 3)));
            std::string mats;
            for (size_t k = 0; k < r->parts.size(); k++) {
                if (k) mats += ", ";
                const Material* m = (k < r->materials.size()) ? r->materials[k] : NULL;
                mats += m ? m->name : (r->parts[k].material.empty() ? std::string(T("(default)")) : r->parts[k].material);
            }
            out.push_back(W3dResourceData(T("Parts"), IntegerText((long)r->parts.size()) + (mats.empty() ? std::string() : " (" + mats + ")")));
            out.push_back(W3dResourceData(T("Vertex groups"), IntegerText((long)r->signatureGroups.size())));
            out.push_back(W3dResourceData(T("Shape keys"), IntegerText((long)r->signatureShapes)));
            // SKINNEADA: algun objeto que la usa se deforma con un armature (el skinning es del OBJETO; los
            // grupos de vertices solos -una seleccion guardada, un grupo que lee Lua- no la hacen skinneada)
            bool skin = false;
            std::vector<Object*> us; Users(id, us);
            for (size_t k = 0; k < us.size() && !skin; k++) skin = ((const Mesh*)us[k])->skinArmature != NULL;
            out.push_back(W3dResourceData(T("Skinned"), IfNo(skin)));
            out.push_back(W3dResourceData(T("CPU memory"), W3dBytesText((double)r->Bytes())));
            out.push_back(W3dResourceData(T("GPU memory"), r->vboShow ? W3dBytesText((double)r->Bytes()) : std::string(T("not uploaded"))));
        } else out.push_back(W3dResourceData(T("State"), T("not loaded")));
        const long size = FileSize(r->entry);
        if (size >= 0) out.push_back(W3dResourceData(T("In the .w3d"), W3dBytesText((double)size)));
        out.push_back(W3dResourceData(T("Users"), IntegerText(W3dMeshResourceUsersInScene(r))));
    }
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        MeshResource* r = LiveMesh(id);
        if (!r) return false;
        const std::string n = W3dMeshFreeName(W3dNameNormalize(newValue, "Mesh"), r);
        if (final) *final = n;
        if (n == r->name) return true;
        // el MISMO paso de undo que el rename de la tarjeta "3D Mesh" (por serial: el registro se purga)
        UndoCaptureRename(W3dDestGlobal(W3dRenameDest::MeshG, r->serial));
        r->name = n;
        return true;
    }
    bool Delete(const std::string& id, std::string* reason) {
        MeshResource* r = LiveMesh(id);
        if (!r) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        if (W3dMeshResourceUsersInScene(r) > 0) { if (reason) *reason = "It is in use"; return false; }
        // un paso de undo todavia lo nombra (un objeto borrado con Ctrl+Z): deja de ser del proyecto ya y
        // se va cuando el undo lo suelte (si un Ctrl+Z le devuelve el objeto, vuelve)
        if (!W3dMeshResourceDelete(r)) r->deleted = true;
        return true;
    }
    void Users(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        MeshResource* r = LiveMesh(id);
        if (!r) return;
        std::vector<Object*> objs;
        ObjectsScene(objs);
        for (size_t i = 0; i < objs.size(); i++)
            if (objs[i]->getType() == ObjectType::mesh && ((Mesh*)objs[i])->mesh == r) out.push_back(objs[i]);
    }
};

// ============================================================================
//  PROVEEDOR: MATERIALES (la lista global Materials, sin los de la UI ni el por defecto)
// ============================================================================
// las mallas huerfanas cuyos materiales NO se pudieron leer (entrada rota): no se reintenta por
// cuadro (por serial; se olvida al invalidar: abrir, guardar)
static std::set<int> gUnreadableMats;
// usuarios de cada material: cada MALLA de la escena que lo usa en alguna parte cuenta una
// vez, y cada malla HUERFANA del registro (se guarda con sus materiales) tambien
static void UsersMaterials(std::map<const Material*, int>& count) {
    std::vector<Object*> objs;
    ObjectsScene(objs);
    std::map<const MeshResource*, int> useMesh;
    for (size_t i = 0; i < objs.size(); i++) {
        if (objs[i]->getType() != ObjectType::mesh) continue;
        const Mesh* m = (const Mesh*)objs[i];
        if (m->mesh) useMesh[m->mesh]++;
        std::set<const Material*> seen;
        for (size_t g = 0; g < m->materialsGroup.size(); g++) {
            const Material* mat = m->materialsGroup[g].material;
            if (mat && seen.insert(mat).second) count[mat]++;
            for (size_t c = 0; c < m->materialsGroup[g].layersMat.size(); c++) {   // (las capas de encima tambien lo usan)
                const Material* mc = m->materialsGroup[g].layersMat[c];
                if (mc && seen.insert(mc).second) count[mc]++;
            }
        }
    }
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++) {
        if (useMesh[reg[i]] > 0) continue;
        // huerfana: sus materiales (resueltos aunque no este cargada; se cachean en el recurso)
        if (reg[i]->materials.empty() && !reg[i]->entry.empty() && !gUnreadableMats.count(reg[i]->serial))
            if (!W3dMeshResourceMaterials(reg[i], NULL)) gUnreadableMats.insert(reg[i]->serial);
        std::set<const Material*> seen;
        for (size_t k = 0; k < reg[i]->materials.size(); k++) {
            const Material* mat = reg[i]->materials[k];
            if (mat && seen.insert(mat).second) count[mat]++;
        }
    }
}
// los materiales DEL PROYECTO (arriba de la marca de la UI, sin el por defecto). Antes de
// listar, un material PURGADO que un undo volvio a poner en una malla vuelve a la lista.
static void MaterialsProject(std::vector<Material*>& out, std::map<const Material*, int>& count) {
    UsersMaterials(count);
    const std::vector<Material*>& pur = MaterialsPurged();
    std::vector<Material*> revive;
    for (size_t i = 0; i < pur.size(); i++) if (count[pur[i]] > 0) revive.push_back(pur[i]);
    for (size_t i = 0; i < revive.size(); i++) MaterialRevive(revive[i]);
    out.clear();
    // (los de una LIBRERIA externa no son del proyecto: se ven en la vista de su libreria)
    for (size_t i = (size_t)MaterialsBase(); i < Materials.size(); i++)
        if (Materials[i] && Materials[i] != MaterialDefault && Materials[i]->lib.empty()) out.push_back(Materials[i]);
}
static Material* MaterialByName(const std::string& n) {
    for (size_t i = (size_t)MaterialsBase(); i < Materials.size(); i++)
        if (Materials[i] && Materials[i] != MaterialDefault && Materials[i]->name == n) return Materials[i];
    return NULL;
}

// el paso de undo de un material CREADO por la biblioteca ("Duplicate", el "New Material" del "+"): deshacer lo
// PURGA (queda vivo en el cementerio de materiales) y rehacer lo revive
struct UndoCreatedMat { Material* m; bool live; };
static void UndoCreatedMatApply(void* d) {
    UndoCreatedMat* u = (UndoCreatedMat*)d;
    if (u->live) { if (MaterialPurge(u->m)) u->live = false; }
    else if (MaterialRevive(u->m)) u->live = true;
    W3dResourcesViewInvalidate();
}
static void UndoCreatedMatFree(void* d) { delete (UndoCreatedMat*)d; }
// un material NUEVO de la biblioteca (copia de 'de', o uno en blanco), con su paso de undo
static Material* NewMaterialWithUndo(const std::string& base, const Material* of) {
    Material* n = FactoryMaterial(new Material(MaterialFreeName(W3dNameNormalize(base, "Material"), NULL)));
    if (of) {
        const std::string name = n->name;
        *n = *of;                  // (las texturas se COMPARTEN: cada ranura de la copia retiene la suya)
        n->name = name;
        TextureRetain(n->texture);
        TextureRetain(n->normalTexture);
        TextureRetain(n->textureB);
        for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) TextureRetain(n->mapTex[k]);
        for (size_t c = 0; c < n->layers.size(); c++) TextureRetain(n->layers[c].tex);
    }
    UndoCreatedMat* u = new UndoCreatedMat();
    u->m = n; u->live = true;
    UndoExternal f; f.apply = UndoCreatedMatApply; f.free = UndoCreatedMatFree;
    UndoPushExternal(f, u);
    return n;
}
Material* W3dLibraryNewMaterial() { NoRoot sr; return NewMaterialWithUndo("Material", NULL); }
// una COPIA de 'de' con nombre libre a partir de 'base' (con Ctrl+Z): el horneado de la luz la usa para no ponerle la luz de
// una malla a un material que comparten otras
Material* W3dMaterialCopyWithUndo(const Material* of, const std::string& base) { NoRoot sr; return NewMaterialWithUndo(base, of); }

// el paso de undo del borrado de un material EN USO: las partes que lo usaban vuelven a el (y el
// material vuelve de su cementerio: W3dResourcesCheckPurged). Rehacer las vuelve a dejar sin el.
struct UndoRemovedMat {
    Material* m;
    std::vector<Mesh*> meshes; std::vector<int> parts;
    bool removed;
};
static void UndoRemovedMatApply(void* d) {
    UndoRemovedMat* u = (UndoRemovedMat*)d;
    for (size_t i = 0; i < u->meshes.size(); i++) {
        Mesh* m = u->meshes[i];
        if (!m || u->parts[i] < 0 || u->parts[i] >= (int)m->materialsGroup.size()) continue;
        m->materialsGroup[(size_t)u->parts[i]].material = u->removed ? u->m : MaterialDefault;
    }
    if (u->removed) { MaterialRevive(u->m); u->removed = false; }
    else { MaterialPurge(u->m); u->removed = true; }
    W3dResourcesViewInvalidate();
}
static void UndoRemovedMatUnlink(void* d, Object* deleted) {
    UndoRemovedMat* u = (UndoRemovedMat*)d;
    for (size_t i = 0; i < u->meshes.size(); i++) if ((Object*)u->meshes[i] == deleted) u->meshes[i] = NULL;
}
static void UndoRemovedMatFree(void* d) { delete (UndoRemovedMat*)d; }

class MaterialProvider : public W3dResourceProvider {
public:
    static void Item(const Material* m, int users, W3dResourceItem& it) {
        it.id = m->name;
        it.name = m->name;
        it.folder = m->folder;
        it.icon = (int)IconType::material;
        it.users = users;
        it.entry.clear();
        // su textura base (la cargada, o la que espera en la cola / dormida)
        it.info = m->texture ? BaseFromPath(m->texture->path) : BaseFromPath(PendingTextureOf(m));
    }
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        std::vector<Material*> mats; std::map<const Material*, int> count;
        MaterialsProject(mats, count);
        for (size_t i = 0; i < mats.size(); i++) {
            W3dResourceItem it;
            Item(mats[i], count[mats[i]], it);
            out.push_back(it);
        }
    }
    bool Info(const std::string& id, W3dResourceItem& out) {
        Material* m = MaterialByName(id);
        if (!m) return false;
        std::map<const Material*, int> count;
        UsersMaterials(count);
        Item(m, count[m], out);
        return true;
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        Material* m = MaterialByName(id);
        if (!m) return false;
        m->folder = folder;
        return true;
    }
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        Material* m = MaterialByName(id);
        if (!m) return false;
        const std::string n = MaterialFreeName(W3dNameNormalize(newValue, "Material"), m);
        if (final) *final = n;
        if (n == m->name) return true;
        int im = -1;
        for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == m) { im = (int)k; break; }
        // el MISMO paso de undo que "Rename Material" de la tarjeta (indice en la lista global)
        if (im >= 0) UndoCaptureRename(W3dDestGlobal(W3dRenameDest::MaterialG, im));
        m->name = n;
        return true;
    }
    bool Delete(const std::string& id, std::string* reason) {
        Material* m = MaterialByName(id);
        if (!m) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        std::map<const Material*, int> count;
        UsersMaterials(count);
        if (count[m] > 0) { if (reason) *reason = "It is in use"; return false; }
        // los materiales ANIMADOS lo modifican: no es un huerfano
        for (size_t i = 0; i < AnimatedMaterials.size(); i++) {
            const AnimatedMaterial* am = AnimatedMaterials[i];
            if (!am) continue;
            for (size_t t = 0; t < am->targets.size(); t++)
                if (am->targets[t] == m) { if (reason) *reason = "An animated material uses it"; return false; }
        }
        int im = -1;
        for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == m) { im = (int)k; break; }
        if (!MaterialPurge(m)) { if (reason) *reason = "It can't be deleted"; return false; }
        // la lista global se CORRIO: los renames de material del undo van por indice
        if (im >= 0) UndoDeletedList(W3dDestGlobal(W3dRenameDest::MaterialG, im));
        return true;
    }
    void Users(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        Material* mat = MaterialByName(id);
        if (!mat) return;
        std::vector<Object*> objs;
        ObjectsScene(objs);
        for (size_t i = 0; i < objs.size(); i++) {
            if (objs[i]->getType() != ObjectType::mesh) continue;
            const Mesh* m = (const Mesh*)objs[i];
            bool uses = false;
            for (size_t g = 0; g < m->materialsGroup.size() && !uses; g++) {
                if (m->materialsGroup[g].material == mat) uses = true;
                for (size_t c = 0; c < m->materialsGroup[g].layersMat.size() && !uses; c++)
                    if (m->materialsGroup[g].layersMat[c] == mat) uses = true;   // (como capa de encima)
            }
            if (uses) out.push_back(objs[i]);
        }
    }
    bool Duplicate(const std::string& id, std::string* idNew, std::string* reason) {
        Material* m = MaterialByName(id);
        if (!m) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        Material* n = NewMaterialWithUndo(m->name, m);
        if (idNew) *idNew = n->name;
        return true;
    }
    // EN USO: las partes que lo usan pasan al material POR DEFECTO (con Ctrl+Z) y el material sale de la lista
    bool KnowsDeleteInUse() const { return true; }
    bool DeleteInUse(const std::string& id, std::string* reason) {
        Material* mat = MaterialByName(id);
        if (!mat) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        if (!MaterialDefault) MaterialDefault = FactoryMaterial(new Material("Default Material", true));
        UndoRemovedMat* u = new UndoRemovedMat();
        u->m = mat; u->removed = false;
        std::vector<Object*> objs;
        ObjectsScene(objs);
        for (size_t i = 0; i < objs.size(); i++) {
            if (objs[i]->getType() != ObjectType::mesh) continue;
            Mesh* m = (Mesh*)objs[i];
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                if (m->materialsGroup[g].material == mat) { u->meshes.push_back(m); u->parts.push_back((int)g); }
                // como CAPA de encima: la capa se va (no puede quedar apuntando a un material borrado)
                std::vector<Material*>& cm = m->materialsGroup[g].layersMat;
                for (size_t c = 0; c < cm.size(); ) { if (cm[c] == mat) cm.erase(cm.begin() + (long)c); else c++; }
            }
        }
        UndoRemovedMatApply(u);   // (deja las partes en el por defecto y purga el material)
        UndoExternal f; f.apply = UndoRemovedMatApply; f.unlink = UndoRemovedMatUnlink; f.free = UndoRemovedMatFree;
        UndoPushExternal(f, u);
        return true;
    }
    void Data(const std::string& id, std::vector<W3dResourceData>& out) {
        out.clear();
        Material* m = MaterialByName(id);
        if (!m) return;
        W3dResourceItem it;
        Info(id, it);
        const std::string tex = m->texture ? BaseFromPath(m->texture->path) : BaseFromPath(PendingTextureOf(m));
        out.push_back(W3dResourceData(T("Texture"), tex.empty() ? std::string(T("(none)")) : tex));
        if (m->normalTexture) out.push_back(W3dResourceData(T("Normal map"), BaseFromPath(m->normalTexture->path)));
        if (m->textureB) out.push_back(W3dResourceData(T("Texture B"), BaseFromPath(m->textureB->path)));
        { static const char* const kN[W3D_MAPS] = { "", "", "Emission Map", "Roughness Map", "Metallic Map", "Alpha Map" };
          for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++)
              if (m->mapTex[k]) out.push_back(W3dResourceData(T(kN[k]), BaseFromPath(m->mapTex[k]->path))); }
        if (!m->layers.empty()) out.push_back(W3dResourceData(T("Texture layers"), IntegerText((long)m->layers.size())));
        out.push_back(W3dResourceData(T("Transparent"), IfNo(m->transparent)));
        out.push_back(W3dResourceData(T("Users"), IntegerText(it.users)));
    }
};

// ============================================================================
//  PROVEEDOR: TEXTURAS (las cargadas + las entradas textures/ del contenedor)
// ============================================================================
// LA CLAVE de una textura = su ruta NORMALIZADA, igual que la del cache (TextureCache.cpp):
// separador '/', sin "./" ni "dir/../". Una entrada "textures/x.png" es su propia clave.
// la ruta YA esta en esa forma (el caso de casi todas: "textures/piso.png"): la biblioteca normaliza miles de
// rutas por listado y el camino largo parte cada una en tramos
static bool KeyAlreadyNormal(const std::string& r) {
    if (r.empty()) return true;
    for (size_t i = 0; i < r.size(); i++) {
        const char c = r[i];
        if (c == '\\') return false;
        if (c == '/' && i + 1 < r.size() && r[i + 1] == '/') return false;   // "//"
        if (c == '.' && (i == 0 || r[i - 1] == '/')) {
            // un tramo que empieza con '.': "." y ".." se resuelven; ".oculto" es un nombre
            const size_t end = r.find('/', i);
            const size_t n = (end == std::string::npos ? r.size() : end) - i;
            if (n == 1 || (n == 2 && r[i + 1] == '.')) return false;
        }
    }
    return r[r.size() - 1] != '/';
}
static std::string KeyTexture(std::string r) {
    if (KeyAlreadyNormal(r)) return r;
    for (size_t i = 0; i < r.size(); i++) if (r[i] == '\\') r[i] = '/';
    std::vector<std::string> segments;
    std::string t;
    const bool abs = !r.empty() && r[0] == '/';
    for (size_t i = 0; i <= r.size(); i++) {
        const char c = (i < r.size()) ? r[i] : '/';
        if (c != '/') { t += c; continue; }
        if (t.empty() || t == ".") { t.clear(); continue; }
        if (t == ".." && !segments.empty() && segments.back() != "..") segments.pop_back();
        else segments.push_back(t);
        t.clear();
    }
    std::string out = abs ? "/" : "";
    for (size_t i = 0; i < segments.size(); i++) { if (i) out += '/'; out += segments[i]; }
    return out;
}

// carpeta cosmetica de cada textura (por clave) y las entradas purgadas en la sesion
static std::map<std::string, std::string> gFolderTex;
static std::set<std::string> gPurgedTex;
// ANTES de la ingesta del guardado: (textura, su clave) de las que tienen carpeta
static std::vector<std::pair<Texture*, std::string> > gTexPrev;

// LO DE LA BIBLIOTECA QUE VIVE AFUERA (a proposito: "ext:"). La MARCA de externa (W3dContainer.h) guarda la
// ruta tal cual la resolvio quien la cargo (al abrir, "ext:piso.png" queda "./piso.png") y la biblioteca
// nombra todo NORMALIZADO ("piso.png"): estas miran las marcas por clave.
static bool ExternalMarked(const std::string& key) {
    std::vector<std::string> ext; W3dExternalRefList(&ext);
    for (size_t i = 0; i < ext.size(); i++) if (KeyTexture(ext[i]) == key) return true;
    return false;
}
// la ruta MARCADA de esa clave, tal cual ("" = no esta marcada)
static std::string ExternalPathMarked(const std::string& key) {
    std::vector<std::string> ext; W3dExternalRefList(&ext);
    for (size_t i = 0; i < ext.size(); i++) if (KeyTexture(ext[i]) == key) return ext[i];
    return std::string();
}
// suelta TODAS las marcas de esa clave; devuelve las que solto (para volver a ponerlas si algo falla)
static std::vector<std::string> UnmarkExternal(const std::string& key) {
    std::vector<std::string> ext, loose; W3dExternalRefList(&ext);
    for (size_t i = 0; i < ext.size(); i++)
        if (KeyTexture(ext[i]) == key) { W3dExternalRefUnmark(ext[i]); loose.push_back(ext[i]); }
    return loose;
}
// las TEXTURAS de afuera que son de la biblioteca: las que se pasaron afuera desde ella y las que el proyecto
// guardo como suyas ("externos"). Una imagen externa que solo nombra un HUD (.w3dui) no es de aca: por eso
// no alcanza con las marcas. Si nadie la usa sigue siendo del proyecto (un huerfano: se conserva).
static std::set<std::string> gExternalTexBib;

// LOS SCRIPTS: una textura que solo nombra un .lua (setTextura(obj, "textures/hud.png")) NO
// es huerfana. El texto de los scripts se lee una vez y se re-escanea al invalidar (abrir,
// guardar, purgar), no por cuadro.
static bool gScriptsRead = false;
static std::vector<std::string> gScriptsText;
// cuantos scripts nombran cada nombre de archivo (ScriptsThatName), recordado mientras el texto de los
// scripts sea el mismo: buscar cada textura y cada sonido en TODO el texto de los scripts en cada listado
// costaba decenas de ms con un proyecto grande (y la biblioteca se lista en cada cuadro)
static std::map<std::string, int> gScriptsName;
static std::vector<std::string> gEntriesTex;   // las entradas textures/ del contenedor montado
static bool gEntriesRead = false;

static void CollectPathsScripts(Object* o, std::set<std::string>& out) {
    if (!o) return;
    if (o->scriptData)
        for (size_t i = 0; i < o->scriptData->scripts.size(); i++)
            if (!o->scriptData->scripts[i].path.empty()) out.insert(o->scriptData->scripts[i].path);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectPathsScripts(o->Childrens[i], out);
}
static void EnsureScripts() {
    if (gScriptsRead) return;
    gScriptsRead = true;
    gScriptsText.clear();
    gScriptsName.clear();   // (otro texto: lo recordado ya no vale)
    std::set<std::string> paths;
    std::vector<std::string> ents;
    W3dContainerListFolder("scripts/", ents);
    for (size_t i = 0; i < ents.size(); i++) paths.insert(ents[i]);
    { std::vector<Object*> roots; W3dLiveRoots(roots);
      for (size_t i = 0; i < roots.size(); i++) CollectPathsScripts(roots[i], paths); }
    for (std::set<std::string>::iterator it = paths.begin(); it != paths.end(); ++it) {
        const std::string& r = *it;
        if (r.size() < 4 || r.compare(r.size() - 4, 4, ".lua") != 0) continue;
        std::vector<unsigned char> d;
        if (!w3dFileSystem::ReadFileBytes(r, d) || d.empty()) continue;
        gScriptsText.push_back(std::string((const char*)&d[0], d.size()));
    }
}
static void EnsureEntriesTex() {
    if (gEntriesRead) return;
    gEntriesRead = true;
    W3dContainerListFolder("textures/", gEntriesTex);
}
// un caracter que puede ser parte de un nombre de archivo (el nombre no termina ni empieza ahi)
static bool CharByName(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '.' || (unsigned char)c >= 0x80;
}
// el texto nombra el archivo 'base' ENTERO: "otrodosmats.png" o "dosmats.png.bak" no nombran
// "dosmats.png" (la ruta de adelante, "textures/dosmats.png", si)
static bool NamesFile(const std::string& text, const std::string& base) {
    for (size_t p = text.find(base); p != std::string::npos; p = text.find(base, p + 1)) {
        const size_t end = p + base.size();
        if ((p == 0 || !CharByName(text[p - 1])) && (end >= text.size() || !CharByName(text[end])))
            return true;
    }
    return false;
}
// cuantos scripts nombran la textura (por su nombre de archivo: el script puede usar la
// entrada entera o solo el nombre)
static int ScriptsThatName(const std::string& key) {
    EnsureScripts();
    const std::string base = BaseFromPath(key);
    if (base.empty()) return 0;
    std::map<std::string, int>::iterator it = gScriptsName.find(base);
    if (it != gScriptsName.end()) return it->second;
    int n = 0;
    for (size_t i = 0; i < gScriptsText.size(); i++)
        if (NamesFile(gScriptsText[i], base)) n++;
    gScriptsName[base] = n;
    return n;
}

// las rutas DORMIDAS de un material (import_obj.h: las de un huerfano recien abierto, que no se
// cargan hasta que una malla lo use): su normal map y sus capas. La base ya la da PendingTextureOf.
// Escribibles: el rename de una textura las sigue.
static void PathsAsleep(const Material* m, std::vector<std::string*>& out) {
    out.clear();
    AsleepTex* d = AsleepTexturesOf(m);
    if (!d) return;
    if (!d->normal.empty()) out.push_back(&d->normal);
    for (size_t c = 0; c < d->layers.size(); c++) if (!d->layers[c].texture.empty()) out.push_back(&d->layers[c].texture);
}

// UNA pasada que junta: las texturas cargadas del proyecto y sus usuarios (por clave). Los
// usuarios son materiales (textura base -tambien la que espera en la cola-, normal map, capas,
// cuadros de material animado, y las rutas DORMIDAS de un huerfano), particulas, elementos 2D y la
// fuente bitmap de un texto 2D (por ruta), y los scripts.
struct TexUse {
    int users;
    std::vector<Object*> objects;   // solo si se pidieron
    TexUse() : users(0) {}
};
// la FUENTE BITMAP de un texto 2D es un PAR: el png del atlas y su .json de glifos. El lector
// (Font2D.cpp) acepta la ruta de cualquiera de los dos y deriva el otro por el NOMBRE (misma
// ruta, otra extension), asi que el que el texto no nombra TAMBIEN esta en uso: si no, "Purge
// Orphans" lo borraba y la fuente dejaba de cargar. Devuelve la clave del hermano solo si es una
// entrada textures/ del contenedor (el .json que el guardado clasifica en extra/ no es de esta
// vista); "" = no hay.
static std::string FontSibling(const std::string& key) {
    const size_t point = key.find_last_of('.'), bar = key.find_last_of('/');
    if (point == std::string::npos || (bar != std::string::npos && point < bar)) return std::string();
    std::string ext = key.substr(point);
    for (size_t i = 0; i < ext.size(); i++) if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] + 32);
    std::string h;
    if (ext == ".png") h = key.substr(0, point) + ".json";
    else if (ext == ".json") h = key.substr(0, point) + ".png";
    else return std::string();
    EnsureEntriesTex();
    for (size_t i = 0; i < gEntriesTex.size(); i++) if (KeyTexture(gEntriesTex[i]) == h) return h;
    return std::string();
}
static void UsersTextures(std::map<std::string, TexUse>& use, bool withObjects) {
    // materiales -> que claves usan
    std::map<const Material*, std::set<std::string> > byMat;
    for (size_t i = 0; i < Materials.size(); i++) {
        const Material* m = Materials[i];
        if (!m || !m->lib.empty()) continue;   // (uno de una LIBRERIA: sus texturas son de ella)
        std::set<std::string>& s = byMat[m];
        if (m->texture && !m->texture->path.empty()) s.insert(KeyTexture(m->texture->path));
        else { const std::string p = PendingTextureOf(m); if (!p.empty()) s.insert(KeyTexture(p)); }
        if (m->normalTexture && !m->normalTexture->path.empty()) s.insert(KeyTexture(m->normalTexture->path));
        if (m->textureB && !m->textureB->path.empty()) s.insert(KeyTexture(m->textureB->path));
        for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++)
            if (m->mapTex[k] && !m->mapTex[k]->path.empty()) s.insert(KeyTexture(m->mapTex[k]->path));
        for (size_t c = 0; c < m->layers.size(); c++)
            if (m->layers[c].tex && !m->layers[c].tex->path.empty()) s.insert(KeyTexture(m->layers[c].tex->path));
        std::vector<std::string*> rd; PathsAsleep(m, rd);
        for (size_t k = 0; k < rd.size(); k++) s.insert(KeyTexture(*rd[k]));
    }
    for (size_t i = 0; i < AnimatedMaterials.size(); i++) {
        const AnimatedMaterial* am = AnimatedMaterials[i];
        if (!am || am->targets.empty()) continue;
        std::set<std::string>& s = byMat[am->targets[0]];
        for (size_t f = 0; f < am->frameTextures.size(); f++)
            if (am->frameTextures[f] && !am->frameTextures[f]->path.empty()) s.insert(KeyTexture(am->frameTextures[f]->path));
    }
    for (std::map<const Material*, std::set<std::string> >::iterator it = byMat.begin(); it != byMat.end(); ++it)
        for (std::set<std::string>::iterator k = it->second.begin(); k != it->second.end(); ++k) use[*k].users++;
    // el FONDO del render (tarjeta Render: su textura 360 y las de la lista): las nombra el .w3d y el fondo las dibuja
    for (size_t i = 0; i < g_renderBgTexs.size(); i++)
        if (g_renderBgTexs[i] && !g_renderBgTexs[i]->path.empty()) use[KeyTexture(g_renderBgTexs[i]->path)].users++;
    // los FLIPBOOKS con nombre (assets de la escena): su atlas va tal cual al guardado, asi que purgarlo
    // dejaria al proyecto nombrando una entrada que no esta (y el guardado se frenaria)
    for (size_t i = 0; i < SceneFlipbooks.size(); i++)
        if (SceneFlipbooks[i] && !SceneFlipbooks[i]->atlas.empty()) use[KeyTexture(SceneFlipbooks[i]->atlas)].users++;
    // los PEDIDOS del STREAMING (io/Streaming.h): una instancia cargada -o cargandose- retiene la textura aunque su
    // material ya no la tenga en la ranura (el usuario le puso otra): es un usuario, no se borra debajo de ella
    { std::vector<std::string> ids;
      W3dStreamingResourcesRetained(W3DREC_TEXTURE, ids);
      for (size_t i = 0; i < ids.size(); i++) use[KeyTexture(ids[i])].users++; }
    // objetos de la escena que las nombran por RUTA (o por sus materiales, para "Select Users")
    std::vector<Object*> objs;
    ObjectsScene(objs);
    for (size_t i = 0; i < objs.size(); i++) {
        Object* o = objs[i];
        std::string r;
        switch (o->getType().v) {
            case ObjectType::particles: r  = ((Particles*)o)->texture; break;
            case ObjectType::image2d:   r  = ((Image2D*)o)->texture; break;
            case ObjectType::slice9:     r = ((Slice9*)o)->texture; break;
            // la SONDA DE REFLEJO: su imagen 360 horneada (tambien la de una de tiempo real que se horneo antes: el .w3d
            // la sigue nombrando). Sin esto Purge Orphans se la llevaba y el guardado se cancelaba por la entrada que falta
            case ObjectType::reflectionProbe: r = ((ReflectionProbe*)o)->texture; break;
            case ObjectType::text2d: {
                r = ((Text2D*)o)->fontBitmap;   // el png del atlas de glifos (o su .json)
                // y el HERMANO del par (el .json del png, o el png del .json)
                const std::string h = r.empty() ? std::string() : FontSibling(KeyTexture(r));
                if (!h.empty()) {
                    TexUse& uh = use[h];
                    uh.users++;
                    if (withObjects) uh.objects.push_back(o);
                }
                break;
            }
            case ObjectType::button2d:
                r = ((Button2D*)o)->textureBackground;
                // su ICONO es otra imagen: cuenta aparte (el fondo va por el camino comun de abajo)
                if (!((Button2D*)o)->icon.empty() && KeyTexture(((Button2D*)o)->icon) != KeyTexture(r)) {
                    TexUse& ui = use[KeyTexture(((Button2D*)o)->icon)];
                    ui.users++;
                    if (withObjects) ui.objects.push_back(o);
                }
                break;
            case ObjectType::mesh:
                if (withObjects) {
                    const Mesh* m = (const Mesh*)o;
                    std::set<std::string> ks;
                    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                        std::map<const Material*, std::set<std::string> >::iterator pm = byMat.find(m->materialsGroup[g].material);
                        if (pm != byMat.end()) ks.insert(pm->second.begin(), pm->second.end());
                    }
                    for (std::set<std::string>::iterator k = ks.begin(); k != ks.end(); ++k) use[*k].objects.push_back(o);
                }
                break;
            default: break;
        }
        if (r.empty()) continue;
        TexUse& u = use[KeyTexture(r)];
        u.users++;
        if (withObjects) u.objects.push_back(o);
    }
}

static void ReleasePreviewViewOf(const std::string& key);   // (el recurso activo, mas abajo)
static void ForgetFailedPreviewView();                      // idem

// ---------------------------------------------------------------------------
//  RENOMBRAR UNA TEXTURA = MUDAR SU ENTRADA DEL CONTENEDOR
//  El nombre de una textura ES su archivo: renombrarla es darle otra entrada ("textures/piso.png"
//  -> "textures/madera.png") y llevar ahi a todos los que la nombran. Los bytes van al overlay
//  del contenedor montado con el nombre nuevo (los de una importada en la sesion se MUDAN, sin
//  copia); la entrada vieja del zip queda como PURGADA (el guardado no la conserva). Solo se
//  renombran las texturas que ya son del proyecto (una entrada); las de disco, despues de guardar.
// ---------------------------------------------------------------------------
static bool TextureRenamable(const std::string& key) {
    return W3dContainerHasMounted() && W3dIsEntryName(key);
}
// la textura CARGADA con esa clave (NULL = no esta en memoria)
static Texture* LoadedTextureOf(const std::string& key) {
    for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++)
        if (Textures[i] && KeyTexture(Textures[i]->path) == key) return Textures[i];
    return NULL;
}
// una entrada que no se puede tomar: esta en el contenedor (y no se purgo), cargada, o en la cola
static bool EntryTextureTaken(const std::string& e) {
    if (W3dContainerExists(e) && !gPurgedTex.count(e)) return true;
    if (LoadedTextureOf(e)) return true;
    for (size_t i = 0; i < Materials.size(); i++) {
        const std::string p = Materials[i] ? PendingTextureOf(Materials[i]) : std::string();
        if (!p.empty() && KeyTexture(p) == e) return true;
        std::vector<std::string*> rd; PathsAsleep(Materials[i], rd);
        for (size_t k = 0; k < rd.size(); k++) if (KeyTexture(*rd[k]) == e) return true;
    }
    return false;
}
// la entrada que PIDE 'nuevo' para la textura 'clave': misma carpeta del contenedor y misma
// extension (el contenido no cambia de formato), el nombre como slug de entrada
static std::string EntryTextureRequested(const std::string& key, const std::string& newValue) {
    const size_t bar = key.find_last_of('/');
    const std::string dir = (bar == std::string::npos) ? std::string() : key.substr(0, bar + 1);
    const std::string file = BaseFromPath(key);
    const size_t point = file.find_last_of('.');
    const std::string ext = (point == std::string::npos || point == 0) ? std::string() : file.substr(point);
    std::string base = BaseFromPath(W3dNameNormalize(newValue, file.c_str()));
    // "madera.png" pedido para un .png: la extension ya esta (sin distinguir mayusculas)
    if (!ext.empty() && base.size() > ext.size()) {
        std::string queue = base.substr(base.size() - ext.size());
        for (size_t i = 0; i < queue.size(); i++) if (queue[i] >= 'A' && queue[i] <= 'Z') queue[i] = (char)(queue[i] - 'A' + 'a');
        std::string e2 = ext;
        for (size_t i = 0; i < e2.size(); i++) if (e2[i] >= 'A' && e2[i] <= 'Z') e2[i] = (char)(e2[i] - 'A' + 'a');
        if (queue == e2) base = base.substr(0, base.size() - ext.size());
    }
    return dir + W3dSlugEntry(base) + ext;
}
// la pedida hecha LIBRE ("madera-2.png", como las importadas)
static std::string EntryFreeTexture(const std::string& key, const std::string& newValue) {
    const std::string requested = EntryTextureRequested(key, newValue);
    if (requested == key || !EntryTextureTaken(requested)) return requested;
    const size_t point = requested.find_last_of('.'), bar = requested.find_last_of('/');
    const bool withExt = (point != std::string::npos && (bar == std::string::npos || point > bar + 1));
    const std::string root = withExt ? requested.substr(0, point) : requested;
    const std::string ext = withExt ? requested.substr(point) : std::string();
    for (int n = 2; n < 10000; n++) {
        char b[16]; sprintf(b, "-%d", n);
        const std::string c = root + b + ext;
        if (c == key || !EntryTextureTaken(c)) return c;
    }
    return key;
}
// cambia la ruta en un string que nombra la textura (particula, imagen 2D, flipbook...)
static void RenamePath(std::string& r, const std::string& old, const std::string& newValue) {
    if (!r.empty() && KeyTexture(r) == old) r = newValue;
}
static void RenamePathsObjects(Object* o, const std::string& old, const std::string& newValue) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        switch (h->getType().v) {
            case ObjectType::particles: RenamePath(((Particles*)h)->texture, old, newValue); break;
            case ObjectType::image2d:   RenamePath(((Image2D*)h)->texture, old, newValue); break;
            case ObjectType::slice9:     RenamePath(((Slice9*)h)->texture, old, newValue); break;
            case ObjectType::text2d:    RenamePath(((Text2D*)h)->fontBitmap, old, newValue); break;
            case ObjectType::button2d:
                RenamePath(((Button2D*)h)->textureBackground, old, newValue);
                RenamePath(((Button2D*)h)->icon, old, newValue);
                break;
            default: break;
        }
        RenamePathsObjects(h, old, newValue);
    }
}
// algun Text2D usa esa textura como FUENTE BITMAP (el png de su atlas de glifos)
static bool FontThatNames(Object* o, const std::string& key) {
    if (!o) return false;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::text2d) {
            const std::string& f = ((Text2D*)h)->fontBitmap;
            if (!f.empty() && KeyTexture(f) == key) return true;
        }
        if (FontThatNames(h, key)) return true;
    }
    return false;
}
// el .json de glifos de la fuente bitmap 'png' en el contenedor, buscado como lo busca el lector
// (Fuente2D): al lado del png ("textures/x.json") o donde lo clasifica el guardado ("extra/x.json").
// "" = no hay (o ya no es del proyecto).
static std::string FontJson(const std::string& png) {
    const size_t point = png.find_last_of('.'), bar = png.find_last_of('/');
    const bool withExt = (point != std::string::npos && (bar == std::string::npos || point > bar + 1));
    const std::string js = (withExt ? png.substr(0, point) : png) + ".json";
    if (W3dContainerExists(js) && !gPurgedTex.count(js)) return js;
    const std::string alt = "extra/" + BaseFromPath(js);
    if (W3dContainerExists(alt) && !gPurgedTex.count(alt)) return alt;
    return std::string();
}
// el .json de la fuente viaja con su png: mismo lugar donde estaba, con el nombre del png NUEVO.
// Devuelve la entrada vieja que dejo de ser del proyecto ("" = no habia json, o no se pudo mudar).
static std::string MoveSourceJson(const std::string& oldPng, const std::string& newPng, std::string* newJson) {
    const std::string jv = FontJson(oldPng);
    if (jv.empty()) return std::string();
    const size_t b = jv.find_last_of('/');
    std::string base   = BaseFromPath(newPng);
    const size_t point = base.find_last_of('.');
    if (point != std::string::npos && point > 0) base = base.substr(0, point);
    const std::string jn = ((b == std::string::npos) ? std::string() : jv.substr(0, b + 1)) + base + ".json";
    if (jn == jv) return std::string();
    bool ok;
    if (W3dContainerEntryEdited(jv)) ok = W3dContainerMoveEdited(jv, jn);
    else {
        std::vector<unsigned char> d;
        ok = w3dFileSystem::ReadFileBytes(jv, d) && !d.empty() && W3dContainerWriteEntry(jn, &d[0], d.size());
    }
    if (!ok) {
        w3dLogfW("[outliner] no pude mudar el json de glifos %s -> %s: la fuente cae a la de siempre", jv.c_str(), jn.c_str());
        return std::string();
    }
    if (newJson) *newJson = jn;
    return jv;
}
// TODOS los que nombran la textura 'viejo' pasan a nombrar 'nuevo' (sin tocar bytes): la cargada 't' (misma
// ficha del cache, otra clave), la cola de carga diferida (y las rutas DORMIDAS de un material huerfano), los
// objetos 2D / particulas / la fuente bitmap de un texto 2D y los flipbooks; y su carpeta cosmetica la sigue.
// Lo usan el rename (una entrada que se muda) y el pase adentro/afuera del .w3d (entrada <-> archivo de disco).
static void ChangeTexturePath(Texture* t, const std::string& old, const std::string& newValue) {
    if (t) TextureRename(t, newValue);
    for (size_t i = 0; i < Materials.size(); i++) {
        std::string* p = Materials[i] ? PendingTextureRefOf(Materials[i]) : NULL;
        if (p) RenamePath(*p, old, newValue);
        std::vector<std::string*> rd; PathsAsleep(Materials[i], rd);   // (normal map y capas dormidas)
        for (size_t k = 0; k < rd.size(); k++) RenamePath(*rd[k], old, newValue);
    }
    { std::vector<Object*> roots; W3dLiveRoots(roots);   // (las escenas/prefabs que no se miran tambien)
      for (size_t i = 0; i < roots.size(); i++) RenamePathsObjects(roots[i], old, newValue); }
    for (size_t i = 0; i < SceneFlipbooks.size(); i++)
        if (SceneFlipbooks[i]) RenamePath(SceneFlipbooks[i]->atlas, old, newValue);
    std::map<std::string, std::string>::iterator c = gFolderTex.find(old);
    if (c != gFolderTex.end()) { const std::string ch = c->second; gFolderTex.erase(c); gFolderTex[newValue] = ch; }
    else gFolderTex.erase(newValue);
    ForgetFailedPreviewView();
    W3dResourcesViewInvalidate();   // el listado de entradas del contenedor cambio
}
// ACTUALIZAR TEXTURA (la biblioteca y el click derecho en el material): la imagen se edito afuera y se vuelve a leer.
// Adentro del .w3d: del archivo de disco del que se copio (W3dAssetOrigin), que pisa la copia de adentro; afuera: de su
// ruta. La textura cargada se sube de nuevo en el lugar (los materiales que la usan la ven cambiada).
// false + motivo (en INGLES, la UI lo pasa por T()) si no se pudo
bool W3dTextureUpdate(const std::string& key, std::string* reason, std::string* detail) {
    if (key.empty()) return false;
    if (W3dIsEntryName(key) && W3dContainerExists(key)) {
        const std::string source = W3dAssetOrigin(key);
        if (source.empty()) { if (reason) *reason = "The original file of the texture is not known"; return false; }
        std::vector<unsigned char> d;
        if (!w3dFileSystem::ReadFileBytes(source, d) || d.empty()) {
            if (reason) *reason = "Original file not found";
            if (detail) *detail = source;
            return false;
        }
        if (!W3dContainerWriteEntry(key, &d[0], d.size())) { if (reason) *reason = "Could not read the image"; return false; }
    } else if (!w3dFileSystem::FileExists(key)) {
        if (reason) *reason = "Original file not found";
        if (detail) *detail = key;
        return false;
    }
    Texture* t = LoadedTextureOf(key);
    if (t && !TextureReread(t)) { if (reason) *reason = "Could not read the image"; return false; }
    ReleasePreviewViewOf(key);   // (la vista previa de la biblioteca y lo leido del archivo se rehacen)
    gInfoArch = InfoFile();
    { extern void RTInvalidate(); RTInvalidate(); }   // (el trazado de rayos la vuelve a leer)
    { extern bool g_redraw; g_redraw = true; }
    return true;
}

// la clave de biblioteca de una textura cargada ("" = ninguna)
std::string W3dTextureKey(const Texture* t) { return (t && !t->path.empty()) ? KeyTexture(t->path) : std::string(); }
// ACTUALIZAR desde la UI (la biblioteca, el click derecho en el material): actualiza cada una y avisa como salio
void W3dTexturesUpdateNotifying(const std::vector<std::string>& keys) {
    int ok = 0;
    std::string last, error;
    for (size_t i = 0; i < keys.size(); i++) {
        std::string reason, detail;
        if (W3dTextureUpdate(keys[i], &reason, &detail)) { ok++; last = keys[i]; continue; }
        if (error.empty()) {
            error = std::string(T("Texture not reloaded")) + ": " + W3dShortName(BaseFromPath(keys[i])) + ". " + T(reason.c_str());
            if (!detail.empty()) error += ": " + detail;
        }
    }
    if (!error.empty()) { Notify(error, true); return; }
    if (ok == 1) Notify(std::string(T("Texture reloaded")) + ": " + W3dShortName(BaseFromPath(last)), false);
    else if (ok > 1) { char b[96]; snprintf(b, sizeof(b), T("%d textures reloaded"), ok); Notify(b, false); }
}

// LA MUDANZA (exacta: 'nuevo' ya viene libre). La usan el rename y su undo (que muda de vuelta).
static bool MoveTexture(const std::string& old, const std::string& newValue) {
    if (old == newValue || !TextureRenamable(old) || !W3dIsEntryName(newValue)) return false;
    Texture* t = LoadedTextureOf(old);
    // (el undo no puede volver a un nombre que mientras tanto tomo OTRA textura cargada)
    Texture* other = LoadedTextureOf(newValue);
    if (other && other != t) return false;
    // 1) LOS BYTES: la editada en la sesion se muda; la del zip se copia al overlay. Una textura
    //    creada en memoria (pintura, "nueva textura") no tiene bytes todavia: el guardado los pide
    //    por su ruta (TexEditBytesToSave), asi que alcanza con cambiarle la ruta.
    if (W3dContainerEntryEdited(old)) {
        if (!W3dContainerMoveEdited(old, newValue)) return false;
    } else {
        std::vector<unsigned char> d;
        if (w3dFileSystem::ReadFileBytes(old, d) && !d.empty()) {
            if (!W3dContainerWriteEntry(newValue, &d[0], d.size())) return false;
        } else if (!t) return false;
    }
    // 1b) una FUENTE BITMAP (la nombra un Text2D): su .json de glifos va con ella. El lector lo
    //     deriva del nombre del png; si se quedara con el nombre viejo, la fuente dejaria de cargar
    std::string oldJson, newJson;
    if (InAnyRoot(FontThatNames, old)) oldJson = MoveSourceJson(old, newValue, &newJson);
    // 2) la entrada vieja deja de ser del proyecto (el guardado no la conserva); la nueva si
    gPurgedTex.insert(old);
    gPurgedTex.erase(newValue);
    W3dAssetOriginMove(old, newValue);   // (el archivo del que salio sigue siendo el mismo: Actualizar textura)
    if (!oldJson.empty()) { gPurgedTex.insert(oldJson); gPurgedTex.erase(newJson); }
    // 3) TODOS los que la nombran y su carpeta
    ChangeTexturePath(t, old, newValue);
    w3dLogf("[outliner] textura renombrada: %s -> %s", old.c_str(), newValue.c_str());
    return true;
}
// el paso de undo: el toggle de siempre entre los dos nombres (muda de vuelta)
struct UndoRowTexture { std::string current, other; };
static void UndoRowTextureApply(void* d) {
    UndoRowTexture* u = (UndoRowTexture*)d;
    if (!MoveTexture(u->current, u->other)) return;
    W3dActiveRenamedResource(W3D_VIEW_TEXTURES, u->current, u->other);
    std::swap(u->current, u->other);
}
static void UndoRowTextureFree(void* d) { delete (UndoRowTexture*)d; }

// ---------------------------------------------------------------------------
//  PURGAR UNA TEXTURA CARGADA (en caliente): sale de la GPU y de las listas; el objeto va al
//  cementerio del cache (TexturePurge) hasta cerrar el proyecto, asi ningun puntero viejo queda
//  colgado. La pintura que tenia sin guardar se descarta (era un huerfano: se confirmo).
// ---------------------------------------------------------------------------
static bool PurgeTextureLoaded(Texture* t) {
    if (!t || !TexturePurge(t)) return false;
    TexEditFind(t);             // la textura ya no esta viva: la pintura suelta sus pixeles
    return true;
}

// ============================================================================
//  LOS ARCHIVOS (sonidos, scripts, fuentes, videos): sus carpetas cosmeticas, lo purgado en la
//  sesion y a quien nombran. Su id es su RUTA normalizada (la entrada "sounds/paso.wav", o la
//  ruta de disco de uno que vive afuera del .w3d), como el de las texturas.
// ============================================================================
static std::map<std::string, std::string> gFolderArch;   // clave -> carpeta cosmetica
static std::set<std::string> gPurgedArch;                // entradas purgadas / mudadas / pasadas afuera
static std::map<int, std::vector<std::string> > gEntriesArch;   // tipo -> entradas de su carpeta del contenedor
static bool gEntriesArchRead = false;

// la carpeta del contenedor de cada tipo de archivo
static const char* TypeFolder(int type) {
    switch (type) {
        case W3D_VIEW_SOUNDS: return "sounds/";
        case W3D_VIEW_SCRIPTS: return "scripts/";
        case W3D_VIEW_FONTS: return "fonts/";
        case W3D_VIEW_VIDEOS:  return "videos/";
        case W3D_VIEW_SHADERS: return "shaders/";
        case W3D_VIEW_TEXTURES: return "textures/";
    }
    return "";
}
static void EnsureEntriesArch() {
    if (gEntriesArchRead) return;
    gEntriesArchRead = true;
    gEntriesArch.clear();
    const int types[5] = { W3D_VIEW_SOUNDS, W3D_VIEW_SCRIPTS, W3D_VIEW_FONTS, W3D_VIEW_VIDEOS, W3D_VIEW_SHADERS };
    for (int k = 0; k < 5; k++) W3dContainerListFolder(TypeFolder(types[k]), gEntriesArch[types[k]]);
}
// el TIPO de archivo por su extension (lo que el contenedor clasificaria): -1 = no es de estos
static int FileTypeFromPath(const std::string& r) {
    const std::string c = W3dCategoryByExtension(r);
    if (c == "sounds") return W3D_VIEW_SOUNDS;
    if (c == "scripts") return W3D_VIEW_SCRIPTS;
    if (c == "fonts") return W3D_VIEW_FONTS;
    if (c == "videos")  return W3D_VIEW_VIDEOS;
    if (c == "shaders") return W3D_VIEW_SHADERS;
    return -1;
}

// las REFERENCIAS de los objetos a un archivo de ese tipo (una por objeto): los scripts de cada
// objeto, la fuente de un texto 2D, el archivo de un video 2D. 'fn' recibe el string que la guarda.
struct RefFile { Object* obj; std::string* path; };
static void CollectRefsFile(Object* o, int type, std::vector<RefFile>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (type == W3D_VIEW_SCRIPTS && h->scriptData)
            for (size_t k = 0; k < h->scriptData->scripts.size(); k++) {
                RefFile r; r.obj = h; r.path = &h->scriptData->scripts[k].path;
                if (!r.path->empty()) out.push_back(r);
            }
        if (type == W3D_VIEW_FONTS && h->getType() == ObjectType::text2d && !((Text2D*)h)->source.empty()) {
            RefFile r; r.obj = h; r.path = &((Text2D*)h)->source; out.push_back(r);
        }
        if (type == W3D_VIEW_VIDEOS && h->getType() == ObjectType::video2d && !((Video2D*)h)->video.empty()) {
            RefFile r; r.obj = h; r.path = &((Video2D*)h)->video; out.push_back(r);
        }
        // un SHADER lo usan las mallas cuyos materiales lo nombran (la referencia es la del material)
        if (type == W3D_VIEW_SHADERS && h->getType() == ObjectType::mesh) {
            Mesh* me = (Mesh*)h;
            for (size_t k = 0; k < me->materialsGroup.size(); k++) {
                Material* mt = me->materialsGroup[k].material;
                if (mt && !mt->shaderFile.empty()) { RefFile r; r.obj = h; r.path = &mt->shaderFile; out.push_back(r); }
            }
        }
        if (type == W3D_VIEW_SOUNDS && h->getType() == ObjectType::sound && !((Sound*)h)->file.empty()) {
            RefFile r; r.obj = h; r.path = &((Sound*)h)->file; out.push_back(r);
        }
        CollectRefsFile(h, type, out);
    }
}
static void RefsFile(int type, std::vector<RefFile>& out) {
    out.clear();
    std::vector<Object*> roots; W3dLiveRoots(roots);
    for (size_t i = 0; i < roots.size(); i++) CollectRefsFile(roots[i], type, out);
}

// ============================================================================
//  DENTRO / FUERA DEL .w3d (texturas y archivos). La ruta de un recurso que vive AFUERA es su ruta
//  de disco (en el archivo, "ext:" relativa al .w3d); la de uno de ADENTRO, su entrada. Pasar de uno a
//  otro cambia la ruta de TODOS los que lo nombran (como un rename), y el paso de undo vuelve.
// ============================================================================
// todos los que nombran 'viejo' (del tipo) pasan a nombrar 'nuevo'; la carpeta cosmetica lo sigue
static void ChangeFilePath(int type, const std::string& old, const std::string& newValue) {
    if (type == W3D_VIEW_TEXTURES) { ChangeTexturePath(LoadedTextureOf(old), old, newValue); return; }
    std::vector<RefFile> refs;
    RefsFile(type, refs);
    for (size_t i = 0; i < refs.size(); i++) if (KeyTexture(*refs[i].path) == old) *refs[i].path = newValue;
    std::map<std::string, std::string>::iterator c = gFolderArch.find(old);
    if (c != gFolderArch.end()) { const std::string ch = c->second; gFolderArch.erase(c); gFolderArch[newValue] = ch; }
    else gFolderArch.erase(newValue);
    W3dResourcesViewInvalidate();
}
static std::set<std::string>& PurgedOf(int type) { return type == W3D_VIEW_TEXTURES ? gPurgedTex : gPurgedArch; }

// ---------------------------------------------------------------------------
//  LA ENTRADA QUE UN PASO DE UNDO PUEDE DEVOLVER. Pasar un archivo AFUERA y borrar una textura o un archivo
//  EN USO dejan su entrada como PURGADA: el proximo guardado no la escribe. El paso de undo solo recuerda
//  RUTAS, asi que guardar y despues deshacer dejaba a los usuarios nombrando una entrada que ya no estaba en
//  el .w3d (el guardado siguiente la escribia "ext:" como faltante, o el recurso desaparecia; el borrado en
//  uso perdia la UNICA copia). Cada uno de esos pasos RETIENE su entrada: el guardado que la descarta copia
//  antes sus bytes (el contenedor viejo todavia esta montado) y el paso, al volver a nombrarla, la repone en
//  el overlay del contenedor (el proximo guardado la vuelve a escribir). Sin guardado en el medio no se copia
//  nada: la entrada sigue en el contenedor montado.
// ---------------------------------------------------------------------------
struct RetainedEntry {
    std::string entry;
    std::vector<unsigned char> bytes;   // los que tenia en el contenedor que el guardado reemplazo (vacio = no hizo falta)
};
static std::set<RetainedEntry*> gRetained;
// NULL = no es una entrada del contenedor (un archivo de disco: el guardado no lo toca)
static RetainedEntry* RetainEntry(const std::string& e) {
    if (!W3dIsEntryName(e)) return NULL;
    RetainedEntry* r = new RetainedEntry();
    r->entry = e;
    gRetained.insert(r);
    return r;
}
static void ReleaseEntry(RetainedEntry* r) {
    if (!r) return;
    gRetained.erase(r);
    delete r;
}
// el paso vuelve a NOMBRAR la entrada: si un guardado la saco del .w3d, vuelve al overlay con sus bytes
static void RestoreEntry(RetainedEntry* r) {
    if (!r || r->bytes.empty() || W3dContainerExists(r->entry)) return;
    if (!W3dContainerWriteEntry(r->entry, &r->bytes[0], r->bytes.size())) {
        w3dLogfW("[biblioteca] no pude reponer la entrada %s: el recurso queda sin su archivo", r->entry.c_str());
        return;
    }
    w3dLogf("[biblioteca] entrada repuesta por el undo: %s (%u bytes)", r->entry.c_str(), (unsigned)r->bytes.size());
    std::vector<unsigned char>().swap(r->bytes);   // (ya esta en el overlay: otro guardado la vuelve a retener)
    W3dResourcesViewInvalidate();                  // el listado del contenedor cambio
}

// el PASO de dentro/fuera: 'de' es la ruta de ahora, 'a' la otra; 'aOutside' = 'a' es la de disco. 'ret' = su
// entrada (la de adentro), retenida por si un guardado la saca del .w3d
struct UndoLocation { int type; std::string of, a; bool aOutside; RetainedEntry* ret; };
static void MoveLocation(int type, const std::string& of, const std::string& a, bool aOutside) {
    ChangeFilePath(type, of, a);
    std::set<std::string>& pur = PurgedOf(type);
    if (aOutside) {
        W3dExternalRefMark(a);
        if (type == W3D_VIEW_TEXTURES) gExternalTexBib.insert(a);   // (aunque nadie la use sigue en la vista)
        pur.insert(of);               // la entrada ya no es del proyecto (el guardado no la conserva)
    } else {
        UnmarkExternal(of);
        gExternalTexBib.erase(of);
        pur.erase(a);
    }
}
static void UndoLocationApply(void* d) {
    // de la ruta de ahora ('de') a la otra ('a'); despues el paso queda listo para el camino inverso
    UndoLocation* u = (UndoLocation*)d;
    if (!u->aOutside) RestoreEntry(u->ret);   // (vuelve ADENTRO: su entrada tiene que estar en el contenedor)
    MoveLocation(u->type, u->of, u->a, u->aOutside);
    W3dActiveRenamedResource(u->type, u->of, u->a);
    std::swap(u->of, u->a);
    u->aOutside = !u->aOutside;
}
static void UndoLocationFree(void* d) {
    UndoLocation* u = (UndoLocation*)d;
    ReleaseEntry(u->ret);
    delete u;
}

static int LocationFromPath(const std::string& id) {
    if (id.empty()) return -1;
    return W3dIsEntryName(id) ? 0 : 1;
}
static std::string ExternalPathOf(const std::string& id) {
    if (g_w3dDirProject.empty()) return std::string();
    std::string d = g_w3dDirProject;
    if (!d.empty() && d[d.size() - 1] != '/' && d[d.size() - 1] != '\\') d += '/';
    return d + BaseFromPath(id);
}
// PISAR el archivo de afuera si ya existe (Guardar como: el explorador ya pregunto "Sobrescribir archivo?")
bool g_w3dLocationOverwrite = false;
static bool SetLocationFile(int type, const std::string& id, int location, const std::string& extPath,
                                  std::string* idNew, std::string* reason) {
    if (idNew) *idNew = id;
    const int now = LocationFromPath(id);
    if (now < 0) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (now == location) return true;
    if (!W3dContainerHasMounted()) { if (reason) *reason = "Save the project first"; return false; }
    std::vector<unsigned char> data;
    if (!BytesFile(id, data)) { if (reason) *reason = "Its file couldn't be read"; return false; }
    std::string newValue;
    if (location == 1) {
        // AFUERA: el archivo se escribe al lado (o donde se eligio). Si ya hay OTRO archivo con ese
        // nombre no se pisa (podria ser de otro proyecto)
        const std::string path = extPath.empty() ? ExternalPathOf(id) : extPath;
        if (path.empty()) { if (reason) *reason = "Save the project first"; return false; }
        std::vector<unsigned char> has;
        if (!g_w3dLocationOverwrite && w3dFileSystem::FileExists(path) && (!w3dFileSystem::ReadFileBytes(path, has) || has != data)) {
            if (reason) *reason = "A different file already exists there";
            return false;
        }
        if (g_w3dLocationOverwrite) has.clear();   // (se escribe encima)
        if (has.empty()) {
            FILE* f = fopen(path.c_str(), "wb");
            if (!f) { if (reason) *reason = "The file couldn't be written"; return false; }
            const size_t w = fwrite(&data[0], 1, data.size(), f);
            fclose(f);
            if (w != data.size()) { if (reason) *reason = "The file couldn't be written"; return false; }
        }
        newValue = KeyTexture(path);
    } else {
        // ADENTRO: se ingiere al contenedor (dedup por contenido: vuelve a la entrada que tenia si
        // es la misma)
        const std::vector<std::string> loose = UnmarkExternal(id);
        newValue = KeyTexture(W3dImportAsset(id));
        if (newValue == id || !W3dIsEntryName(newValue)) {
            for (size_t i = 0; i < loose.size(); i++) W3dExternalRefMark(loose[i]);
            if (reason) *reason = "It couldn't be copied into the .w3d";
            return false;
        }
    }
    MoveLocation(type, id, newValue, location == 1);
    UndoLocation* u = new UndoLocation();
    u->type = type; u->of = newValue; u->a = id; u->aOutside = (location != 1);
    u->ret = RetainEntry(location == 1 ? id : newValue);   // la de ADENTRO
    UndoExternal f; f.apply = UndoLocationApply; f.free = UndoLocationFree;
    UndoPushExternal(f, u);
    W3dActiveRenamedResource(type, id, newValue);
    if (idNew) *idNew = newValue;
    w3dLogf("[biblioteca] %s -> %s (%s)", id.c_str(), newValue.c_str(), location == 1 ? "outside" : "inside");
    return true;
}
// las filas de "adentro / afuera" y "falta el archivo" de un archivo del proyecto
static void DataLocation(const std::string& id, std::vector<W3dResourceData>& out) {
    const int u = LocationFromPath(id);
    // (DONDE esta no va en los datos: lo dice el desplegable Ubicacion de la tarjeta, justo arriba)
    if (u == 0) {
        const long size = FileSize(id);
        if (size >= 0) out.push_back(W3dResourceData(T("In the .w3d"), W3dBytesText((double)size)));
    } else if (u == 1) {
        out.push_back(W3dResourceData(T("Path"), id));
        const long size = FileSize(id);
        if (size < 0) out.push_back(W3dResourceData(T("Warning"), T("The external file is missing")));
        else out.push_back(W3dResourceData(T("File size"), W3dBytesText((double)size)));
    }
}

// el paso de undo del borrado de una textura EN USO: los materiales y los objetos que la nombraban la
// recuperan (la textura vuelve de su cementerio: W3dResourcesCheckPurged)
struct RemovedTexMat { Material* m; int where; Texture* t; };   // donde: -1 base, -2 normal map, >=0 capa
struct RemovedTexObj { Object* o; int field; std::string path; };   // campo: 0 textura/fondo, 1 icono de un boton
struct UndoRemovedTex {
    std::string key;
    std::vector<RemovedTexMat> mats;
    std::vector<RemovedTexObj> objs;
    bool removed;
    RetainedEntry* ret;   // su entrada (si es de adentro), por si un guardado la saca del .w3d
};
static std::string* FieldTexObj(Object* o, int field) {
    switch (o->getType().v) {
        case ObjectType::particles: return &((Particles*)o)->texture;
        case ObjectType::image2d:   return &((Image2D*)o)->texture;
        case ObjectType::slice9:     return &((Slice9*)o)->texture;
        case ObjectType::text2d:    return &((Text2D*)o)->fontBitmap;
        case ObjectType::button2d:    return field == 1 ? &((Button2D*)o)->icon : &((Button2D*)o)->textureBackground;
        default: return NULL;
    }
}
static void UndoRemovedTexApply(void* d) {
    UndoRemovedTex* u = (UndoRemovedTex*)d;
    const bool set    = u->removed;   // deshacer = ponerla de vuelta
    // (su archivo primero: la textura del cementerio se recarga de su entrada)
    if (set) RestoreEntry(u->ret);
    for (size_t i = 0; i < u->mats.size(); i++) {
        Material* m = u->mats[i].m;
        if (!m) continue;
        Texture* t = set ? u->mats[i].t : NULL;
        if (u->mats[i].where == -1) m->texture = t;
        else if (u->mats[i].where == -2) m->normalTexture = t;
        else if (u->mats[i].where == -3) m->textureB = t;
        else if (u->mats[i].where <= -10 && -10 - u->mats[i].where < W3D_MAPS) m->mapTex[-10 - u->mats[i].where] = t;   // (los mapas)
        else if (u->mats[i].where < (int)m->layers.size()) m->layers[(size_t)u->mats[i].where].tex = t;
    }
    for (size_t i = 0; i < u->objs.size(); i++) {
        if (!u->objs[i].o) continue;
        std::string* r = FieldTexObj(u->objs[i].o, u->objs[i].field);
        if (r) *r = set ? u->objs[i].path : std::string();
    }
    if (set) { gPurgedTex.erase(u->key); W3dResourcesCheckPurged(); }
    else {
        gPurgedTex.insert(u->key);
        for (Texture* t = LoadedTextureOf(u->key); t; t = LoadedTextureOf(u->key)) if (!PurgeTextureLoaded(t)) break;
    }
    u->removed = !u->removed;
    W3dResourcesViewInvalidate();
}
static void UndoRemovedTexUnlink(void* d, Object* deleted) {
    UndoRemovedTex* u = (UndoRemovedTex*)d;
    for (size_t i = 0; i < u->objs.size(); i++) if (u->objs[i].o == deleted) u->objs[i].o = NULL;
}
static void UndoRemovedTexFree(void* d) {
    UndoRemovedTex* u = (UndoRemovedTex*)d;
    ReleaseEntry(u->ret);
    delete u;
}
static void CollectObjsTex(Object* o, const std::string& key, std::vector<RemovedTexObj>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        for (int field = 0; field < 2; field++) {
            std::string* r = FieldTexObj(h, field);
            if (field == 1 && h->getType() != ObjectType::button2d) r = NULL;
            if (r && !r->empty() && KeyTexture(*r) == key) { RemovedTexObj x; x.o = h; x.field = field; x.path = *r; out.push_back(x); }
        }
        CollectObjsTex(h, key, out);
    }
}

// ============================================================================
//  PROVEEDOR: ARCHIVOS (sonidos, scripts, fuentes, videos)
// ============================================================================
class FileProvider : public W3dResourceProvider {
public:
    int type;
    explicit FileProvider(int t) : type(t) {}
    // todas las claves del tipo: las entradas de su carpeta del contenedor + lo que nombran los objetos
    // (y, para los sonidos, nada mas: los nombran los scripts). Sin las purgadas que nadie nombra.
    void Keys(std::map<std::string, int>& use) {
        use.clear();
        EnsureEntriesArch();
        const std::vector<std::string>& ents = gEntriesArch[type];
        for (size_t i = 0; i < ents.size(); i++) {
            use[KeyTexture(ents[i])] += 0;
        }
        std::vector<RefFile> refs;
        RefsFile(type, refs);
        std::set<std::pair<Object*, std::string> > seen;
        for (size_t i = 0; i < refs.size(); i++) {
            const std::string k = KeyTexture(*refs[i].path);
            if (seen.insert(std::make_pair(refs[i].obj, k)).second) use[k]++;
        }
        // un archivo que se paso AFUERA y que nadie nombra (un sonido: lo nombran los scripts) sigue siendo del
        // proyecto (antes de las purgadas: uno de afuera que se saco de sus usuarios tampoco va)
        std::vector<std::string> ext; W3dExternalRefList(&ext);
        for (size_t i = 0; i < ext.size(); i++) {
            const std::string k = KeyTexture(ext[i]);
            if (FileTypeFromPath(k) == type && !use.count(k)) use[k] = 0;
        }
        for (std::set<std::string>::iterator p = gPurgedArch.begin(); p != gPurgedArch.end(); ++p) {
            std::map<std::string, int>::iterator u = use.find(*p);
            if (u != use.end() && u->second == 0) use.erase(u);
        }
    }
    int UsersOf(const std::string& k, int fromObjects) {
        // los sonidos (y lo que un script nombre por su nombre de archivo) cuentan los scripts que lo nombran
        if (type == W3D_VIEW_SCRIPTS || type == W3D_VIEW_SHADERS) return fromObjects;
        return fromObjects + ScriptsThatName(k);
    }
    void Item(const std::string& k, int users, W3dResourceItem& it) {
        it.id = k;
        it.name = BaseFromPath(k);
        std::map<std::string, std::string>::iterator c = gFolderArch.find(k);
        it.folder = (c != gFolderArch.end()) ? c->second : std::string();
        it.entry  = k;
        it.icon = W3dIconView(type);
        it.users = users;
        it.renamable    = W3dContainerHasMounted() && W3dIsEntryName(k) && type != W3D_VIEW_SCRIPTS;
        const long size = FileSize(k);
        it.info = size >= 0 ? W3dBytesText((double)size) : std::string(T("missing"));
        it.bytes = size;
        it.loaded = users > 0;   // (un archivo se lee cuando alguien lo usa)
    }
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        std::map<std::string, int> use;
        Keys(use);
        for (std::map<std::string, int>::iterator it = use.begin(); it != use.end(); ++it) {
            W3dResourceItem item;
            Item(it->first, UsersOf(it->first, it->second), item);
            out.push_back(item);
        }
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        if (folder.empty()) gFolderArch.erase(id); else gFolderArch[id] = folder;
        return true;
    }
    std::string IdRequest(const std::string& id, const std::string& newValue) { return EntryTextureRequested(id, newValue); }
    // renombrar = MUDAR la entrada (como una textura) y llevar ahi a los objetos que la nombran
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        if (!W3dContainerHasMounted() || !W3dIsEntryName(id) || type == W3D_VIEW_SCRIPTS) return false;
        const std::string destination = EntryFreeTexture(id, newValue);
        if (final) *final = destination;
        if (destination == id) return true;
        const int scripts = ScriptsThatName(id);
        if (!MoveEntryFile(id, destination)) return false;
        UndoRowFile* u = new UndoRowFile();
        u->type = type; u->current = destination; u->other = id;
        UndoExternal f; f.apply = UndoRowFileApply; f.free = UndoRowFileFree;
        UndoPushExternal(f, u);
        if (scripts > 0) {
            char b[16]; sprintf(b, "%d", scripts);
            Notify(std::string(T("Scripts that name the file")) + " (" + b + "): " +
                      W3dShortName(BaseFromPath(id)) + " -> " + W3dShortName(BaseFromPath(destination)), true);
        }
        return true;
    }
    // la mudanza de la entrada (bytes al overlay con el nombre nuevo; la vieja queda purgada)
    bool MoveEntryFile(const std::string& old, const std::string& newValue) {
        if (W3dContainerEntryEdited(old)) {
            if (!W3dContainerMoveEdited(old, newValue)) return false;
        } else {
            std::vector<unsigned char> d;
            if (!BytesFile(old, d) || !W3dContainerWriteEntry(newValue, &d[0], d.size())) return false;
        }
        gPurgedArch.insert(old);
        gPurgedArch.erase(newValue);
        ChangeFilePath(type, old, newValue);
        return true;
    }
    struct UndoRowFile { int type; std::string current, other; };
    static void UndoRowFileApply(void* d);
    static void UndoRowFileFree(void* d) { delete (UndoRowFile*)d; }
    bool Delete(const std::string& id, std::string* reason) {
        std::map<std::string, int> use;
        Keys(use);
        std::map<std::string, int>::iterator u = use.find(id);
        if (u == use.end()) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        if (UsersOf(id, u->second) > 0) { if (reason) *reason = "It is in use"; return false; }
        gPurgedArch.insert(id);
        gFolderArch.erase(id);
        UnmarkExternal(id);   // (una de afuera deja de ser del proyecto; el archivo de disco no se toca)
        W3dResourcesViewInvalidate();
        return true;
    }
    // EN USO: los objetos que lo nombran lo SUELTAN (una fuente vuelve a la de Whisk3D, un video queda
    // vacio), con Ctrl+Z; los scripts que lo nombran por texto quedan como estan (se avisa)
    bool KnowsDeleteInUse() const { return type != W3D_VIEW_SCRIPTS && type != W3D_VIEW_SHADERS; }
    bool DeleteInUse(const std::string& id, std::string* reason) {
        if (type == W3D_VIEW_SCRIPTS || type == W3D_VIEW_SHADERS) { if (reason) *reason = "It is in use"; return false; }
        UndoRemovedArch* u = new UndoRemovedArch();
        u->type = type; u->key = id; u->removed = false;
        u->ret = RetainEntry(id);
        std::vector<RefFile> refs;
        RefsFile(type, refs);
        for (size_t i = 0; i < refs.size(); i++)
            if (KeyTexture(*refs[i].path) == id) { u->objs.push_back(refs[i].obj); u->paths.push_back(*refs[i].path); }
        UndoRemovedArchApply(u);
        UndoExternal f; f.apply = UndoRemovedArchApply; f.unlink = UndoRemovedArchUnlink; f.free = UndoRemovedArchFree;
        UndoPushExternal(f, u);
        const int scripts = ScriptsThatName(id);
        if (scripts > 0) {
            char b[16]; sprintf(b, "%d", scripts);
            Notify(std::string(T("Scripts that name the file")) + " (" + b + "): " + W3dShortName(BaseFromPath(id)), true);
        }
        return true;
    }
    // ('ret' = su entrada, si es de adentro: un guardado la puede sacar del .w3d antes del Ctrl+Z)
    struct UndoRemovedArch { int type; std::string key; std::vector<Object*> objs; std::vector<std::string> paths; bool removed;
                            RetainedEntry* ret; };
    static void UndoRemovedArchApply(void* d) {
        UndoRemovedArch* u = (UndoRemovedArch*)d;
        const bool set = u->removed;
        if (set) RestoreEntry(u->ret);
        for (size_t i = 0; i < u->objs.size(); i++) {
            Object* o = u->objs[i];
            if (!o) continue;
            std::string* r = NULL;
            if (u->type == W3D_VIEW_FONTS && o->getType() == ObjectType::text2d) r = &((Text2D*)o)->source;
            if (u->type == W3D_VIEW_VIDEOS && o->getType() == ObjectType::video2d) r = &((Video2D*)o)->video;
            if (r) *r = set ? u->paths[i] : std::string();
        }
        if (set) gPurgedArch.erase(u->key); else gPurgedArch.insert(u->key);
        u->removed = !u->removed;
        W3dResourcesViewInvalidate();
    }
    static void UndoRemovedArchUnlink(void* d, Object* deleted) {
        UndoRemovedArch* u = (UndoRemovedArch*)d;
        for (size_t i = 0; i < u->objs.size(); i++) if (u->objs[i] == deleted) u->objs[i] = NULL;
    }
    static void UndoRemovedArchFree(void* d) {
        UndoRemovedArch* u = (UndoRemovedArch*)d;
        ReleaseEntry(u->ret);
        delete u;
    }
    void Users(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        std::vector<RefFile> refs;
        RefsFile(type, refs);
        std::set<Object*> seen;
        for (size_t i = 0; i < refs.size(); i++)
            if (KeyTexture(*refs[i].path) == id && seen.insert(refs[i].obj).second) out.push_back(refs[i].obj);
    }
    void Data(const std::string& id, std::vector<W3dResourceData>& out) {
        out.clear();
        const InfoFile& ia = FileInfo(type, id);   // (recordado: se pide en cada cuadro)
        const bool has = ia.bytes > 0;
        const size_t point = id.rfind('.'), bar = id.find_last_of("/\\");
        std::string fmt = (point != std::string::npos && (bar == std::string::npos || point > bar)) ? id.substr(point + 1) : std::string();
        for (size_t i = 0; i < fmt.size(); i++) if (fmt[i] >= 'a' && fmt[i] <= 'z') fmt[i] = (char)(fmt[i] - 32);
        if (type == W3D_VIEW_SOUNDS && has) {
            const W3dSoundData& sd = ia.sound;
            if (ia.isSound) {
                out.push_back(W3dResourceData(T("Format"), sd.format));
                if (sd.frequency > 0) out.push_back(W3dResourceData(T("Sample rate"), IntegerText(sd.frequency) + " Hz"));
                if (sd.channels > 0) out.push_back(W3dResourceData(T("Channels"), sd.channels == 1 ? std::string(T("mono (1)")) :
                                                                  sd.channels == 2 ? std::string(T("stereo (2)")) : IntegerText(sd.channels)));
                if (sd.bits > 0) out.push_back(W3dResourceData(T("Bits"), IntegerText(sd.bits)));
                out.push_back(W3dResourceData(T("Duration"), W3dDurationText(sd.duration)));
                if (sd.format == "WAV") out.push_back(W3dResourceData(T("Loop (smpl)"), IfNo(sd.loop)));
            } else out.push_back(W3dResourceData(T("Format"), fmt));
        } else {
            out.push_back(W3dResourceData(T("Type"), T(W3dViewSingular(type))));
            out.push_back(W3dResourceData(T("Format"), fmt));
        }
        if (has) out.push_back(W3dResourceData(T("Bytes"), W3dBytesText((double)ia.bytes)));
        DataLocation(id, out);
        W3dResourceItem it;
        if (Info(id, it)) out.push_back(W3dResourceData(T("Users"), IntegerText(it.users)));
    }
    int Location(const std::string& id) { return LocationFromPath(id); }
    std::string PathExternalSuggested(const std::string& id) { return ExternalPathOf(id); }
    bool SetLocation(const std::string& id, int location, const std::string& externalPath,
                        std::string* idNew, std::string* reason) {
        return SetLocationFile(type, id, location, externalPath, idNew, reason);
    }
};
static FileProvider gProvSounds(W3D_VIEW_SOUNDS);
static FileProvider gProvScripts(W3D_VIEW_SCRIPTS);
static FileProvider gProvFonts(W3D_VIEW_FONTS);
static FileProvider gProvVideos(W3D_VIEW_VIDEOS);
static FileProvider gProvShaders(W3D_VIEW_SHADERS);
static FileProvider* ProvFile(int type) {
    switch (type) {
        case W3D_VIEW_SOUNDS: return &gProvSounds;
        case W3D_VIEW_SCRIPTS: return &gProvScripts;
        case W3D_VIEW_FONTS: return &gProvFonts;
        case W3D_VIEW_VIDEOS:  return &gProvVideos;
        case W3D_VIEW_SHADERS: return &gProvShaders;
    }
    return NULL;
}
void FileProvider::UndoRowFileApply(void* d) {
    UndoRowFile* u = (UndoRowFile*)d;
    FileProvider* p = ProvFile(u->type);
    if (!p || !p->MoveEntryFile(u->current, u->other)) return;
    W3dActiveRenamedResource(u->type, u->current, u->other);
    std::swap(u->current, u->other);
}

// LA SONDA DE REFLEJO de la escena que usa esa textura (NULL = ninguna). Su imagen no se borra desde la biblioteca, ni
// con "borrar igual": eso suelta la textura de los materiales, pero la sonda la seguia nombrando
static ReflectionProbe* ProbeThatUses(const std::string& key) {
    std::vector<ReflectionProbe*> v; ReflectionProbeList(v);
    for (size_t i = 0; i < v.size(); i++)
        if (v[i] && !v[i]->texture.empty() && KeyTexture(v[i]->texture) == key) return v[i];
    return NULL;
}
static bool ProbeNotLeaves(const std::string& key, std::string* reason) {
    ReflectionProbe* s = ProbeThatUses(key);
    if (!s) return false;
    if (reason) {
        char b[512];
        snprintf(b, sizeof(b), T("It can't be deleted: the reflection probe \"%s\" is using it"), s->name.c_str());
        *reason = b;   // (ya traducido: T() lo devuelve tal cual)
    }
    return true;
}
bool W3dViewRecUntouchable(int view, const std::string& id, std::string* reason) {
    return view == W3D_VIEW_TEXTURES && ProbeNotLeaves(id, reason);
}

class TextureProvider : public W3dResourceProvider {
public:
    // todas las claves de la vista: las cargadas del proyecto, las que esperan en la cola y
    // las entradas textures/ del contenedor; sin las purgadas que siguen sin usuarios
    static void Keys(std::map<std::string, Texture*>& out, std::map<std::string, TexUse>& use) {
        out.clear();
        UsersTextures(use, false);
        for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++) {
            Texture* t = Textures[i];
            if (!t || t->path.empty()) continue;
            if (t->path.compare(0, 4, "lib:") == 0) continue;   // (una de una LIBRERIA externa: no es del proyecto)
            out[KeyTexture(t->path)] = t;
        }
        EnsureEntriesTex();
        for (size_t i = 0; i < gEntriesTex.size(); i++) {
            const std::string k = KeyTexture(gEntriesTex[i]);
            if (!out.count(k)) out[k] = NULL;
        }
        // las que esperan en la cola o que nombra algo y no estan cargadas (externas)
        for (std::map<std::string, TexUse>::iterator it = use.begin(); it != use.end(); ++it)
            if (!out.count(it->first) && it->first.compare(0, 4, "lib:") != 0) out[it->first] = NULL;
        // las de AFUERA de la biblioteca aunque nadie las use (se pasaron afuera siendo huerfanas)
        for (std::set<std::string>::iterator e = gExternalTexBib.begin(); e != gExternalTexBib.end(); ++e)
            if (!out.count(*e) && ExternalMarked(*e)) out[*e] = NULL;
        for (std::set<std::string>::iterator p = gPurgedTex.begin(); p != gPurgedTex.end(); ++p) {
            std::map<std::string, TexUse>::iterator u = use.find(*p);
            if (u == use.end() || u->second.users == 0) out.erase(*p);
        }
    }
    static void Item(const std::string& k, const Texture* t, int users, W3dResourceItem& it) {
        it.id = k;
        it.name = BaseFromPath(k);
        std::map<std::string, std::string>::iterator c = gFolderTex.find(k);
        it.folder = (c != gFolderTex.end()) ? c->second : std::string();
        it.entry  = k;
        it.icon = (int)IconType::texture;
        it.users = users + ScriptsThatName(k);
        // su nombre ES su archivo: renombrarla es mudar su ENTRADA (las de disco, al guardarlas)
        it.renamable = TextureRenamable(k);
        if (t && t->width > 0) it.info = IntegerText(t->width) + "x" + IntegerText(t->height);
        else if (t) it.info.clear();
        else it.info = T("not loaded");
        it.loaded = (t && t->iID);
        long b = -1, bPot = -1; bool from16 = false;
        if (it.loaded && w3dEngine::TextureMemoryOf((unsigned int)t->iID, b, bPot, from16)) it.bytes = b;   // (en la GPU)
        else it.bytes = FileSize(k);                                                                  // (su archivo)
    }
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        std::map<std::string, Texture*> keys; std::map<std::string, TexUse> use;
        Keys(keys, use);
        for (std::map<std::string, Texture*>::iterator it = keys.begin(); it != keys.end(); ++it) {
            W3dResourceItem item;
            std::map<std::string, TexUse>::iterator u = use.find(it->first);
            Item(it->first, it->second, u != use.end() ? u->second.users : 0, item);
            out.push_back(item);
        }
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        if (folder.empty()) gFolderTex.erase(id); else gFolderTex[id] = folder;
        return true;
    }
    bool Delete(const std::string& id, std::string* reason) {
        W3dResourceItem it;
        if (!Info(id, it)) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        if (it.users > 0) { if (reason) *reason = "It is in use"; return false; }
        // la VISTA PREVIA de Properties la pudo cargar solo para mirarla: esa referencia se suelta
        // primero (si nadie mas la tenia, deja de estar cargada)
        ReleasePreviewViewOf(id);
        // CARGADA EN MEMORIA (nadie la usa, pero quedo cargada): se purga EN CALIENTE. Libera la GPU
        // y sale de los selectores; el objeto queda en el cementerio del cache (ver TexturePurge)
        for (Texture* t = LoadedTextureOf(id); t; t = LoadedTextureOf(id))
            if (!PurgeTextureLoaded(t)) {
                if (reason) *reason = "It can't be deleted";
                return false;
            }
        gPurgedTex.insert(id);
        gFolderTex.erase(id);
        // una de AFUERA deja de ser del proyecto (el archivo de disco no se toca)
        if (gExternalTexBib.erase(id)) UnmarkExternal(id);
        return true;
    }
    std::string IdRequest(const std::string& id, const std::string& newValue) {
        return EntryTextureRequested(id, newValue);
    }
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        if (!TextureRenamable(id)) return false;
        const std::string destination = EntryFreeTexture(id, newValue);
        if (final) *final = destination;
        if (destination == id) return true;
        const int scripts = ScriptsThatName(id);   // (antes de mudar: nombran el archivo VIEJO)
        if (!MoveTexture(id, destination)) return false;
        UndoRowTexture* u = new UndoRowTexture();
        u->current = destination; u->other = id;
        UndoExternal f; f.apply = UndoRowTextureApply; f.free = UndoRowTextureFree;
        UndoPushExternal(f, u);
        // los .lua no se reescriben (arman nombres en tiempo de ejecucion): se avisa, como al
        // renombrar una escena que un cambiarEscena() nombra
        if (scripts > 0) {
            char b[16]; sprintf(b, "%d", scripts);
            Notify(std::string(T("Scripts that name the texture")) + " (" + b + "): " +
                      W3dShortName(BaseFromPath(id)) + " -> " + W3dShortName(BaseFromPath(destination)), true);
        }
        return true;
    }
    void Users(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        std::map<std::string, TexUse> use;
        UsersTextures(use, true);
        std::map<std::string, TexUse>::iterator u = use.find(id);
        if (u != use.end()) out = u->second.objects;
    }
    // los datos de la IMAGEN (del archivo, sin cargarla): formato, resolucion, alfa (binario o con
    // transparencias), bytes en el .w3d y en memoria (con mipmaps, un tercio mas), filtrado
    void Data(const std::string& id, std::vector<W3dResourceData>& out) {
        out.clear();
        const InfoFile& ia = FileInfo(W3D_VIEW_TEXTURES, id);   // (recordado: se pide en cada cuadro)
        const W3dImageData& im = ia.im;
        const bool has = ia.isImage;
        const Texture* t = LoadedTextureOf(id);
        const int w = has ? im.width : (t ? t->width : 0), h = has ? im.height : (t ? t->height : 0);
        if (has) out.push_back(W3dResourceData(T("Format"), im.format));
        if (w > 0) out.push_back(W3dResourceData(T("Resolution"), IntegerText(w) + " x " + IntegerText(h)));
        if (has) {
            std::string a = IfNo(im.alpha);
            if (im.alpha && im.alphaType == 1) a += std::string(" (") + T("binary") + ")";
            else if (im.alpha && im.alphaType == 2) a += std::string(" (") + T("with transparency") + ")";
            out.push_back(W3dResourceData(T("Alpha channel"), a));
        }
        if (ia.bytes > 0) out.push_back(W3dResourceData(T("Bytes"), W3dBytesText((double)ia.bytes)));
        if (w > 0) {
            // la memoria de la GPU (RGBA de 8 bits): sin mipmaps y con ellos (la cadena suma un tercio)
            const bool mip = w3dEngine::MipmapsGlobal();
            const double mem = (double)w * (double)h * 4.0;
            out.push_back(W3dResourceData(T("Memory"), W3dBytesText(mem) + " (" + W3dBytesText(mem * 4.0 / 3.0) + " " + T("with mipmaps") + ")"));
            out.push_back(W3dResourceData(T("Mipmaps"), IfNo(mip)));
        }
        out.push_back(W3dResourceData(T("Loaded"), IfNo(t != NULL)));
        DataLocation(id, out);
        W3dResourceItem it;
        if (Info(id, it)) out.push_back(W3dResourceData(T("Users"), IntegerText(it.users)));
    }
    int Location(const std::string& id) { return LocationFromPath(id); }
    std::string PathExternalSuggested(const std::string& id) { return ExternalPathOf(id); }
    bool SetLocation(const std::string& id, int location, const std::string& externalPath,
                        std::string* idNew, std::string* reason) {
        ReleasePreviewViewOf(id);
        return SetLocationFile(W3D_VIEW_TEXTURES, id, location, externalPath, idNew, reason);
    }
    // EN USO: los materiales la sueltan (base, normal map, capas) y los objetos que la nombran por ruta
    // tambien; la textura sale de la GPU al cementerio. Ctrl+Z devuelve todo (la textura revive).
    bool KnowsDeleteInUse() const { return true; }
    bool DeleteInUse(const std::string& id, std::string* reason) {
        if (ProbeNotLeaves(id, reason)) return false;
        for (size_t i = 0; i < SceneFlipbooks.size(); i++)
            if (SceneFlipbooks[i] && !SceneFlipbooks[i]->atlas.empty() && KeyTexture(SceneFlipbooks[i]->atlas) == id) {
                if (reason) *reason = "A flipbook uses it";
                return false;
            }
        ReleasePreviewViewOf(id);
        UndoRemovedTex* u = new UndoRemovedTex();
        u->key = id; u->removed = false;
        u->ret = RetainEntry(id);
        for (size_t i = 0; i < Materials.size(); i++) {
            Material* m = Materials[i];
            if (!m) continue;
            RemovedTexMat x; x.m = m;
            if (m->texture && KeyTexture(m->texture->path) == id) { x.where = -1; x.t = m->texture; u->mats.push_back(x); }
            if (m->normalTexture && KeyTexture(m->normalTexture->path) == id) { x.where = -2; x.t = m->normalTexture; u->mats.push_back(x); }
            if (m->textureB && KeyTexture(m->textureB->path) == id) { x.where = -3; x.t = m->textureB; u->mats.push_back(x); }
            for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++)
                if (m->mapTex[k] && KeyTexture(m->mapTex[k]->path) == id) { x.where = -10 - k; x.t = m->mapTex[k]; u->mats.push_back(x); }
            for (size_t c = 0; c < m->layers.size(); c++)
                if (m->layers[c].tex && KeyTexture(m->layers[c].tex->path) == id) { x.where = (int)c; x.t = m->layers[c].tex; u->mats.push_back(x); }
        }
        { std::vector<Object*> roots; W3dLiveRoots(roots);
          for (size_t i = 0; i < roots.size(); i++) CollectObjsTex(roots[i], id, u->objs); }
        UndoRemovedTexApply(u);
        UndoExternal f; f.apply = UndoRemovedTexApply; f.unlink = UndoRemovedTexUnlink; f.free = UndoRemovedTexFree;
        UndoPushExternal(f, u);
        return true;
    }
};

// ============================================================================
//  PROVEEDOR: ANIMACIONES (los animsets del registro + los que viven solo en memoria)
// ============================================================================
static void UsersAnimSets(std::map<std::string, int>& count, std::map<std::string, std::vector<Object*> >* objs) {
    std::vector<Object*> all;
    ObjectsScene(all);
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i]->getType() != ObjectType::armature) continue;
        Armature* a = (Armature*)all[i];
        // el MISMO criterio que W3dAnimSetUsers (el del guardado)
        if (a->animations.empty()) continue;
        const W3dAnimSet* set = W3dArmatureAnimSetFits(a) ? W3dArmatureAnimSet(a) : 0;
        const std::string n = set ? set->name : a->animSetName;
        if (n.empty()) continue;
        count[n]++;
        if (objs) (*objs)[n].push_back(a);
    }
    // los objetos raiz que la usan como biblioteca de CLIPS DE JERARQUIA (W3dAnimSet.h)
    for (size_t i = 0; i < all.size(); i++) {
        const Object* o = all[i];
        if (!o->clipsHier || o->clipsHier->animset.empty()) continue;
        count[o->clipsHier->animset]++;
        if (objs) (*objs)[o->clipsHier->animset].push_back(all[i]);
    }
}
// "3 clips, 12 bones" / "2 hierarchy clips" (lo que tenga)
static std::string InfoAnimSet(const W3dAnimSet* set) {
    std::string t;
    if (!set->data.clips.empty() || set->data.hierarchies.empty())
        t = Count((long)set->data.clips.size(), "clip", "clips") + ", " +
            Count((long)set->data.bones.size(), "bone", "bones");
    if (!set->data.hierarchies.empty()) {
        if (!t.empty()) t += ", ";
        t += Count((long)set->data.hierarchies.size(), "hierarchy clip", "hierarchy clips");
    }
    return t;
}
// el animset CARGADO con ese nombre (NULL = no esta en memoria)
static W3dAnimSet* AnimSetLoaded(const std::string& name) {
    std::vector<W3dResource*> all;
    W3dResourcesList(W3DREC_ANIMSET, all);
    for (size_t i = 0; i < all.size(); i++)
        if (all[i] && all[i]->state == W3DREC_READY && all[i]->data &&
            ((W3dAnimSet*)all[i]->data)->name == name) return (W3dAnimSet*)all[i]->data;
    return NULL;
}

// paso de undo del renombre de un animset (W3dAnimSetRename no tiene uno propio): el
// toggle de siempre entre los dos nombres
struct UndoRowAnimSet { std::string current, other; };
static void UndoRowAnimSetApply(void* d) {
    UndoRowAnimSet* u = (UndoRowAnimSet*)d;
    std::string remained;
    if (!W3dAnimSetRename(NULL, u->current, u->other, &remained)) return;
    W3dActiveRenamedResource(W3D_VIEW_ANIMATIONS, u->current, remained);
    u->other = u->current;
    u->current = remained;
}
static void UndoRowAnimSetFree(void* d) { delete (UndoRowAnimSet*)d; }

// ---- LOS CLIPS DE JERARQUIA de una biblioteca, como items hijos de su animset (id "Biblioteca/clip") ----
// el clip de un id de clip (NULL = no es uno, o ya no esta); 'rec' = su biblioteca (cargada)
static W3dClipHier* ClipById(const std::string& id, W3dResource** rec) {
    if (rec) *rec = 0;
    const size_t bar = id.find('/');
    if (bar == std::string::npos || bar == 0 || bar + 1 >= id.size()) return 0;
    W3dResource* r = W3dHierLoadedLibrary(id.substr(0, bar));
    W3dAnimSet* set = W3dHierLibrary(r);
    if (!set) return 0;
    const std::string name = id.substr(bar + 1);
    for (size_t i = 0; i < set->data.hierarchies.size(); i++)
        if (set->data.hierarchies[i] && set->data.hierarchies[i]->name == name) { if (rec) *rec = r; return set->data.hierarchies[i]; }
    return 0;
}
static std::string InfoClipHier(const W3dClipHier* c) {
    char b[160];
    snprintf(b, sizeof(b), "%s, %d-%d, %d fps, ", T("hierarchy clip"), c->start, c->end, c->fps);
    std::string t = b;
    t += Count((long)c->tracks.size(), "track", "tracks");
    if (c->retarget == W3D_RETARGET_ROTATIONS) t += std::string(", ") + T("Rotations only");
    return t;
}
// los items de los clips de la biblioteca 'set' (hijos del item 'padre')
static void ListClipsHier(const W3dAnimSet* set, const W3dResourceItem& parent, std::vector<W3dResourceItem>& out) {
    if (!set) return;
    for (size_t k = 0; k < set->data.hierarchies.size(); k++) {
        const W3dClipHier* c = set->data.hierarchies[k];
        if (!c) continue;
        W3dResourceItem it;
        it.id = parent.id + "/" + c->name;
        it.name = c->name;
        it.parent = parent.id;
        it.folder = parent.folder;
        it.entry  = parent.entry;
        it.icon = (int)IconType::object;
        it.users = parent.users;   // (lo usan las mismas raices que a su biblioteca)
        it.info = InfoClipHier(c);
        out.push_back(it);
    }
}

class AnimationProvider : public W3dResourceProvider {
public:
    // clips, duracion, fps y destino (huesos / jerarquia) de la biblioteca cargada, o de UN clip de jerarquia
    void Data(const std::string& id, std::vector<W3dResourceData>& out) {
        out.clear();
        W3dResourceItem it;
        const bool has = Info(id, it);
        W3dClipHier* c = ClipById(id, 0);
        if (c) {
            out.push_back(W3dResourceData(T("Target"), T("hierarchy")));
            out.push_back(W3dResourceData(T("Frames"), IntegerText(c->start) + " - " + IntegerText(c->end)));
            out.push_back(W3dResourceData(T("FPS"), IntegerText(c->fps)));
            out.push_back(W3dResourceData(T("Duration"), W3dDurationText(c->Duration())));
            out.push_back(W3dResourceData(T("Tracks"), IntegerText((long)c->tracks.size())));
        } else {
            const W3dAnimSet* set = AnimSetLoaded(id);
            if (set) {
                float dur = 0.0f; int fps = 0;
                for (size_t k = 0; k < set->data.clips.size(); k++) {
                    const SkeletalAnimation* a = set->data.clips[k];
                    if (!a || a->FrameRate <= 0) continue;
                    const float d = (float)(a->endFrame - a->startFrame + 1) / (float)a->FrameRate;
                    if (d > dur) dur = d;
                    fps = a->FrameRate;
                }
                for (size_t k = 0; k < set->data.hierarchies.size(); k++) {
                    const W3dClipHier* j = set->data.hierarchies[k];
                    if (!j) continue;
                    if (j->Duration() > dur) dur = j->Duration();
                    if (!fps) fps = j->fps;
                }
                std::string destination;
                if (!set->data.clips.empty()) destination = T("bones");
                if (!set->data.hierarchies.empty()) destination += std::string(destination.empty() ? "" : ", ") + T("hierarchy");
                out.push_back(W3dResourceData(T("Clips"), IntegerText((long)(set->data.clips.size() + set->data.hierarchies.size()))));
                if (!destination.empty()) out.push_back(W3dResourceData(T("Target"), destination));
                if (!set->data.bones.empty()) out.push_back(W3dResourceData(T("Bones"), IntegerText((long)set->data.bones.size())));
                out.push_back(W3dResourceData(T("Longest clip"), W3dDurationText(dur)));
                if (fps) out.push_back(W3dResourceData(T("FPS"), IntegerText(fps)));
            } else out.push_back(W3dResourceData(T("State"), T("not loaded")));
            if (has) {
                const long size = FileSize(it.entry);
                if (size >= 0) out.push_back(W3dResourceData(T("In the .w3d"), W3dBytesText((double)size)));
            }
        }
        if (has) out.push_back(W3dResourceData(T("Users"), IntegerText(it.users)));
    }
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        std::map<std::string, int> count;
        UsersAnimSets(count, NULL);
        const std::vector<W3dAnimSetRow>& reg = W3dAnimSetsRegistry();
        std::set<std::string> seen;
        for (size_t i = 0; i < reg.size(); i++) {
            W3dResourceItem it;
            it.id = it.name = reg[i].name;
            it.folder = W3dFolderNormalize(reg[i].folder);
            it.entry  = reg[i].entry;
            it.icon = (int)IconType::animation;
            it.users = count[reg[i].name];
            const W3dAnimSet* set = AnimSetLoaded(reg[i].name);
            if (set) it.info = InfoAnimSet(set);
            else it.info = T("not loaded");
            seen.insert(reg[i].name);
            out.push_back(it);
            ListClipsHier(set, it, out);
        }
        // los animsets EN MEMORIA que todavia no tienen entrada (Alt+D de un armature con clips
        // propios): se ven, pero se ordenan/renombran recien cuando el guardado los registra
        std::vector<W3dResource*> all;
        W3dResourcesList(W3DREC_ANIMSET, all);
        for (size_t i = 0; i < all.size(); i++) {
            if (!all[i] || all[i]->state != W3DREC_READY || !all[i]->data) continue;
            if (all[i]->id.compare(0, 4, "lib:") == 0) continue;   // (uno de una LIBRERIA externa: no es del proyecto)
            const W3dAnimSet* set = (const W3dAnimSet*)all[i]->data;
            if (set->name.empty() || seen.count(set->name)) continue;
            seen.insert(set->name);
            W3dResourceItem it;
            it.id = it.name = set->name;
            it.icon = (int)IconType::animation;
            it.users = count[set->name];
            it.readOnly = true;
            it.info = InfoAnimSet(set) + " (" + T("unsaved") + ")";
            out.push_back(it);
            ListClipsHier(set, it, out);
        }
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        if (ClipById(id, 0)) return false;   // (un clip va donde va su biblioteca)
        return W3dAnimSetSetFolder(id, folder);
    }
    std::string IdRequest(const std::string& id, const std::string& newValue) {
        const size_t bar = id.find('/');
        if (bar != std::string::npos && ClipById(id, 0)) return id.substr(0, bar + 1) + W3dNameNormalize(newValue, "Clip");
        return W3dResourceProvider::IdRequest(id, newValue);
    }
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        // un CLIP DE JERARQUIA: unico entre los de su biblioteca (lo ven todas sus raices), con su paso de undo
        W3dResource* rec = 0;
        W3dClipHier* c = ClipById(id, &rec);
        if (c) {
            W3dClipsViewsSync();   // (la vista que se edita no puede pisarle el nombre al escribirse)
            UndoCaptureClipHier(rec, c);
            W3dHierClipRename(W3dHierLibrary(rec), c, newValue);
            W3dClipActiveViewReread(c);
            if (final) *final = id.substr(0, id.find('/') + 1) + c->name;
            return true;
        }
        std::string remained;
        if (!W3dAnimSetRename(NULL, id, newValue, &remained)) return false;
        if (final) *final = remained;
        if (remained != id) {
            UndoRowAnimSet* u = new UndoRowAnimSet();
            u->current = remained; u->other = id;
            UndoExternal f; f.apply = UndoRowAnimSetApply; f.free = UndoRowAnimSetFree;
            UndoPushExternal(f, u);
        }
        return true;
    }
    bool Delete(const std::string& id, std::string* reason) {
        // un CLIP DE JERARQUIA: sale de su biblioteca (lo dejan de ver todas sus raices), con su paso de undo
        W3dResource* rec = 0;
        W3dClipHier* c = ClipById(id, &rec);
        if (c) {
            if (!UndoClipHierDelete(rec, c)) { if (reason) *reason = "It doesn't exist anymore"; return false; }
            w3dLogf("[W3D] clip de jerarquia borrado: '%s'", id.c_str());
            return true;
        }
        std::map<std::string, int> count;
        UsersAnimSets(count, NULL);
        if (count[id] > 0) { if (reason) *reason = "It is in use"; return false; }
        // CARGADO sin usuarios en la escena = lo retiene un armature BORRADO que vive en el undo (el
        // almacen lo libera con su ultima referencia). Sacarle la fila no lo borraba: volvia enseguida
        // como "(unsaved)" de solo lectura, sin poder purgarse. Igual que una malla en ese caso.
        if (AnimSetLoaded(id)) { if (reason) *reason = "The undo history still uses it"; return false; }
        std::vector<W3dAnimSetRow> reg = W3dAnimSetsRegistry();
        for (size_t i = 0; i < reg.size(); i++) {
            if (reg[i].name != id) continue;
            w3dLogf("[W3D] animset huerfano borrado: '%s' (%s)", id.c_str(), reg[i].entry.c_str());
            reg.erase(reg.begin() + (long)i);
            W3dAnimSetsRegistrySet(reg);
            return true;
        }
        if (reason) *reason = "It isn't saved in the project yet";
        return false;
    }
    void Users(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        std::map<std::string, int> count;
        std::map<std::string, std::vector<Object*> > objs;
        UsersAnimSets(count, &objs);
        // un clip: los de su biblioteca
        const size_t bar = id.find('/');
        out = (bar != std::string::npos && ClipById(id, 0)) ? objs[id.substr(0, bar)] : objs[id];
    }
};

// ============================================================================
//  LO PURGADO QUE VUELVE (un undo lo volvio a poner en uso)
// ============================================================================
static bool MaterialNamesTexture(const Material* m, const Texture* t) {
    if (!m || !t) return false;
    if (m->texture == t || m->normalTexture == t || m->textureB == t) return true;
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) if (m->mapTex[k] == t) return true;
    for (size_t c = 0; c < m->layers.size(); c++) if (m->layers[c].tex == t) return true;
    return false;
}
// (la purgada no tenia referencias -la purga las cero-: la ranura solo toma una de la viva)
static void RepointTexture(Material* m, Texture* of, Texture* a) {
    if (!m) return;
    if (m->texture == of) { TextureRetain(a); m->texture = a; }
    if (m->normalTexture == of) { TextureRetain(a); m->normalTexture = a; }
    if (m->textureB == of) { TextureRetain(a); m->textureB = a; }
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) if (m->mapTex[k] == of) { TextureRetain(a); m->mapTex[k] = a; }
    for (size_t c = 0; c < m->layers.size(); c++) if (m->layers[c].tex == of) { TextureRetain(a); m->layers[c].tex = a; }
}
// cuantas RANURAS de material (y cuadros de materiales animados) apuntan a 't': lo que la textura revivida tiene que
// tener de referencias (TextureRevive la deja en 1; las ranuras son las duenas, Textures.h). Tambien los pasos de
// undo de "poner una textura" que la guardan: cada uno es dueno de una y la suelta al caerse del historial
extern int W3dUndoTextureOwners(const Texture* t);   // (DropResource.cpp)
static int SlotsOf(const Texture* t) {
    int n = W3dUndoTextureOwners(t);
    for (size_t i = 0; i < Materials.size(); i++) {
        const Material* m = Materials[i];
        if (!m) continue;
        if (m->texture == t) n++;
        if (m->normalTexture == t) n++;
        if (m->textureB == t) n++;
        for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) if (m->mapTex[k] == t) n++;
        for (size_t c = 0; c < m->layers.size(); c++) if (m->layers[c].tex == t) n++;
    }
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                if (AnimatedMaterials[i]->frameTextures[f] == t) n++;
    return n;
}
void W3dResourcesCheckPurged() {
    if (MaterialsPurged().empty() && TexturesPurged().empty()) return;
    // 1) los materiales purgados que una malla volvio a usar vuelven a su lista (con un nombre libre)
    if (!MaterialsPurged().empty()) {
        std::map<const Material*, int> count;
        UsersMaterials(count);
        const std::vector<Material*>& pur = MaterialsPurged();
        std::vector<Material*> revive;
        for (size_t i = 0; i < pur.size(); i++) if (count[pur[i]] > 0) revive.push_back(pur[i]);
        for (size_t i = 0; i < revive.size(); i++) MaterialRevive(revive[i]);
    }
    // 2) las texturas purgadas EN CALIENTE que un material vivo vuelve a nombrar: se recargan en el
    //    MISMO objeto; si mientras tanto se cargo otra con esa ruta, el material pasa a usar esa
    const std::vector<Texture*> tp = TexturesPurged();   // (copia: revivir la modifica)
    for (size_t k = 0; k < tp.size(); k++) {
        Texture* t = tp[k];
        bool used  = false;
        for (size_t i = 0; i < Materials.size() && !used; i++) used = MaterialNamesTexture(Materials[i], t);
        for (size_t i = 0; i < AnimatedMaterials.size() && !used; i++)
            if (AnimatedMaterials[i])
                for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size() && !used; f++)
                    used = (AnimatedMaterials[i]->frameTextures[f] == t);
        if (!used) continue;
        Texture* live = TextureFind(t->path);
        if (live) {
            for (size_t i = 0; i < Materials.size(); i++) RepointTexture(Materials[i], t, live);
            for (size_t i = 0; i < AnimatedMaterials.size(); i++)
                if (AnimatedMaterials[i])
                    for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                        if (AnimatedMaterials[i]->frameTextures[f] == t) { TextureRetain(live); AnimatedMaterials[i]->frameTextures[f] = live; }
        } else if (TextureRevive(t)) {
            live = t;
            // revivida con UNA referencia: sus ranuras vuelven a ser sus duenas (una por cada una)
            for (int k2 = SlotsOf(t); k2 > 1; k2--) TextureRetain(t);
        }
        if (live) {
            gPurgedTex.erase(KeyTexture(live->path));   // vuelve a ser del proyecto (el guardado la lleva)
            w3dLogf("[outliner] textura purgada vuelta a usar: %s", live->path.c_str());
        } else {
            w3dLogfW("[outliner] la textura purgada %s se volvio a usar pero su imagen ya no esta", t->path.c_str());
        }
    }
}

// ============================================================================
//  EL REGISTRO DE PROVEEDORES
// ============================================================================
static MeshProvider      gProvMeshes;
// ===== LOS ARCHIVOS QUE FALTAN =====
// referencias a archivos de afuera que NO estan en esta maquina (las que el guardado avisa: "Guardado, pero N file(s)
// external(s) NO estan"). El ORIGEN de una malla importada (el .obj/.fbx del que salio) es solo procedencia: la geometria
// ya viaja adentro del .w3d. Se lista HUERFANO: "Purgar huerfanos" (o Borrar) lo OLVIDA y se terminan los avisos. Lo
// demas que falto en el ultimo guardado (una textura, un sonido de afuera) se lista EN USO por su dueno: se ve que es y
// quien lo usa, y se arregla en el dueno (no se borra a ciegas desde aca)
class MissingProvider : public W3dResourceProvider {
    static void Meshes(Object* o, std::vector<Mesh*>& out) {
        if (!o) return;
        if (o->getType() == ObjectType::mesh) out.push_back((Mesh*)o);
        for (size_t i = 0; i < o->Childrens.size(); i++) Meshes(o->Childrens[i], out);
    }
    // las mallas con un origen que no esta, por origen
    static void Origins(std::map<std::string, std::vector<Mesh*> >& out) {
        out.clear();
        std::vector<Object*> roots; W3dLiveRoots(roots);
        std::vector<Mesh*> ms;
        for (size_t r = 0; r < roots.size(); r++) Meshes(roots[r], ms);
        for (size_t i = 0; i < ms.size(); i++)
            if (!ms[i]->origin.empty() && !w3dFileSystem::FileExists(ms[i]->origin)) out[ms[i]->origin].push_back(ms[i]);
    }
    static std::string Base(const std::string& r) { const size_t b = r.find_last_of("/\\"); return b == std::string::npos ? r : r.substr(b + 1); }
public:
    void List(std::vector<W3dResourceItem>& out) {
        std::map<std::string, std::vector<Mesh*> > org; Origins(org);
        for (std::map<std::string, std::vector<Mesh*> >::iterator it = org.begin(); it != org.end(); ++it) {
            W3dResourceItem i;
            i.id = "origin:" + it->first;
            i.name = "? " + Base(it->first);
            std::string who;
            for (size_t k = 0; k < it->second.size(); k++) who += (k ? ", " : "") + it->second[k]->name;
            i.info = std::string(T("Original file of an imported mesh (only where it came from: the mesh is inside the project)")) +
                     "\n" + who + "\n" + it->first;
            i.icon = (int)IconType::notifError;
            i.users = 0; i.renamable = false; i.loaded = false;
            out.push_back(i);
        }
        for (size_t e = 0; e < g_w3dExternalMissing.size(); e++) {
            const W3dExternal& x = g_w3dExternalMissing[e];
            if (!x.original.empty() && (org.count(x.original) || w3dFileSystem::FileExists(x.original))) continue;
            bool isOrigin = false;   // (un origen que ya se olvido: el guardado viejo todavia lo nombra)
            if (x.who.compare(0, 7, "objeto ") == 0 && !x.original.empty()) {
                std::vector<Object*> roots; W3dLiveRoots(roots);
                std::vector<Mesh*> ms; for (size_t r = 0; r < roots.size(); r++) Meshes(roots[r], ms);
                isOrigin = true;
                for (size_t k = 0; k < ms.size() && isOrigin; k++) if (ms[k]->origin == x.original) isOrigin = false;
            }
            if (isOrigin) continue;
            W3dResourceItem i;
            i.id = "ext:" + x.ref;
            i.name = "? " + Base(x.original.empty() ? x.ref : x.original);
            i.info = x.who + "\n" + (x.original.empty() ? x.ref : x.original);
            i.icon = (int)IconType::notifError;
            i.users = 1; i.readOnly = true; i.renamable = false; i.loaded = false;
            out.push_back(i);
        }
    }
    bool SetFolder(const std::string&, const std::string&) { return false; }
    // OLVIDAR el origen (las mallas que lo nombran): no se toca la geometria
    bool Delete(const std::string& id, std::string* reason) {
        if (id.compare(0, 7, "origin:") != 0) { if (reason) *reason = "It is in use"; return false; }
        const std::string path = id.substr(7);
        std::map<std::string, std::vector<Mesh*> > org; Origins(org);
        std::map<std::string, std::vector<Mesh*> >::iterator it = org.find(path);
        if (it == org.end()) { if (reason) *reason = "It is not missing anymore"; return false; }
        for (size_t k = 0; k < it->second.size(); k++) {
            w3dLogfW("[W3D] '%s': olvido su archivo de origen '%s' (falta; la geometria ya viaja adentro del .w3d)",
                     it->second[k]->name.c_str(), path.c_str());
            it->second[k]->origin.clear();
        }
        return true;
    }
};
static void MeshesOf(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::mesh) out.push_back((Mesh*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) MeshesOf(o->Childrens[i], out);
}
bool W3dPathIsMeshOrigin(const std::string& path) {
    if (path.empty()) return false;
    std::vector<Object*> roots; W3dLiveRoots(roots);
    std::vector<Mesh*> ms;
    for (size_t r = 0; r < roots.size(); r++) MeshesOf(roots[r], ms);
    for (size_t i = 0; i < ms.size(); i++) if (ms[i]->origin == path) return true;
    return false;
}
static MaterialProvider  gProvMaterials;
static TextureProvider    gProvTextures;
static AnimationProvider gProvAnimations;
static MissingProvider      gMissingProv;
static W3dResourceProvider* gProv[W3D_VIEWS] = {
    NULL, &gProvMeshes, &gProvMaterials, &gProvTextures, &gProvAnimations, NULL, NULL, NULL,
    &gProvSounds, &gProvScripts, &gProvFonts, &gProvVideos, &gProvShaders, &gMissingProv
};

void W3dResourcesViewRegister(int view, W3dResourceProvider* p) {
    if (view <= W3D_VIEW_SCENE || view >= W3D_VIEWS) return;   // la escena no es una lista
    gProv[view] = p;
}
W3dResourceProvider* W3dResourcesProviderView(int view) {
    return (view > W3D_VIEW_SCENE && view < W3D_VIEWS) ? gProv[view] : NULL;
}
void W3dViewRecList(int view, std::vector<W3dResourceItem>& out) {
    out.clear();
    W3dResourceProvider* p = W3dResourcesProviderView(view);
    if (!p) return;
    if (view == W3D_VIEW_MATERIALS || view == W3D_VIEW_TEXTURES) W3dResourcesCheckPurged();
    // TODA malla es un recurso: la que nacio en este cuadro (un Add, un import, un script) ya lo tiene
    if (view == W3D_VIEW_MESHES) W3dMeshesEnsureResources();
    p->List(out);
    for (size_t i = 0; i < out.size(); i++) { out[i].folder = W3dFolderNormalize(out[i].folder); out[i].type = view; }
}
bool W3dViewRecInfo(int view, const std::string& id, W3dResourceItem* out) {
    W3dResourceProvider* p = W3dResourcesProviderView(view);
    if (!p || id.empty()) return false;
    W3dResourceItem it;
    if (!p->Info(id, it)) return false;
    it.folder = W3dFolderNormalize(it.folder);
    it.type = view;
    if (out) *out = it;
    return true;
}

// ============================================================================
//  LA BIBLIOTECA
// ============================================================================
// el ORDEN de los tipos en el filtro (el de la lista de mas arriba del .h, sin los que no tienen datos)
static const int kOrderTypes[] = {
    W3D_VIEW_MESHES, W3D_VIEW_MATERIALS, W3D_VIEW_TEXTURES, W3D_VIEW_SOUNDS, W3D_VIEW_ANIMATIONS,
    W3D_VIEW_PREFABS, W3D_VIEW_SCENES, W3D_VIEW_SCRIPTS, W3D_VIEW_SHADERS, W3D_VIEW_FONTS, W3D_VIEW_VIDEOS, W3D_VIEW_LIBS,
    W3D_VIEW_MISSING
};
void W3dLibraryTypes(std::vector<int>& out) {
    out.clear();
    for (size_t i = 0; i < sizeof(kOrderTypes) / sizeof(kOrderTypes[0]); i++)
        if (W3dResourcesProviderView(kOrderTypes[i])) out.push_back(kOrderTypes[i]);
}
void W3dLibraryList(int filter, std::vector<W3dResourceItem>& out) {
    out.clear();
    std::vector<int> types;
    if (filter > W3D_VIEW_SCENE && filter < W3D_VIEWS) types.push_back(filter);
    else W3dLibraryTypes(types);
    std::vector<W3dResourceItem> one;
    for (size_t i = 0; i < types.size(); i++) {
        W3dViewRecList(types[i], one);
        out.insert(out.end(), one.begin(), one.end());
    }
}
std::string W3dBibKey(int type, const std::string& id) { return std::string(W3dViewKey(type)) + ":" + id; }
bool W3dBibByKey(const std::string& c, int* type, std::string* id) {
    const size_t p = c.find(':');
    if (p == std::string::npos) return false;
    const int t = W3dViewByKey(c.substr(0, p));
    if (t <= W3D_VIEW_SCENE) return false;
    if (type) *type = t;
    if (id) *id = c.substr(p + 1);
    return true;
}

// ============================================================================
//  CARPETAS (UN arbol para todos los tipos)
// ============================================================================
static std::vector<std::string> gCreated;   // ordenadas, sin repetir
static std::vector<std::string> gOrderBib;  // el orden MANUAL (W3dBibOrder): claves de biblioteca
static std::map<std::string, int> gOrderIdx; // (su indice, para ordenar rapido)
static bool gOrderIdxReady = false;
static void OrderChange() { gOrderIdxReady = false; }
int W3dBibOrder(const std::string& key) {
    if (!gOrderIdxReady) {
        gOrderIdx.clear();
        for (size_t i = 0; i < gOrderBib.size(); i++) gOrderIdx[gOrderBib[i]] = (int)i;
        gOrderIdxReady = true;
    }
    std::map<std::string, int>::const_iterator it = gOrderIdx.find(key);
    return it == gOrderIdx.end() ? -1 : it->second;
}

std::string W3dFolderNormalize(const std::string& path) { return W3dMeshFolderNormalize(path); }
std::string W3dFolderParent(const std::string& path) {
    size_t p = path.find_last_of('/');
    return (p == std::string::npos) ? std::string() : path.substr(0, p);
}
std::string W3dFolderLeaf(const std::string& path) {
    size_t p = path.find_last_of('/');
    return (p == std::string::npos) ? path : path.substr(p + 1);
}
bool W3dFolderInside(const std::string& path, const std::string& folder) {
    if (folder.empty()) return true;
    if (path == folder) return true;
    return path.size() > folder.size() && path.compare(0, folder.size(), folder) == 0 &&
           path[folder.size()] == '/';
}
const std::vector<std::string>& W3dFoldersCreated() { return gCreated; }
// agrega la carpeta y sus ancestros a 'out'
static void WithAncestors(const std::string& c, std::set<std::string>& out) {
    std::string r = c;
    while (!r.empty()) { out.insert(r); r = W3dFolderParent(r); }
}
void W3dFoldersLibrary(const std::vector<W3dResourceItem>& items, std::vector<std::string>& out) {
    std::set<std::string> s;
    for (size_t i = 0; i < gCreated.size(); i++) WithAncestors(gCreated[i], s);
    for (size_t i = 0; i < items.size(); i++) WithAncestors(items[i].folder, s);
    out.assign(s.begin(), s.end());
}
void W3dFoldersAll(std::vector<std::string>& out) {
    std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
    W3dFoldersLibrary(its, out);
}
static void CreatedAdd(const std::string& c) {
    if (c.empty()) return;
    std::vector<std::string>::iterator it = std::lower_bound(gCreated.begin(), gCreated.end(), c);
    if (it == gCreated.end() || *it != c) gCreated.insert(it, c);
}
std::string W3dFoldersSignature() {
    std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
    return W3dFoldersSignature(its);
}
std::string W3dFoldersSignature(const std::vector<W3dResourceItem>& its) {
    std::string f;
    for (size_t i = 0; i < gCreated.size(); i++) { f += gCreated[i]; f += '\n'; }
    f += '\x03';
    for (size_t i = 0; i < gOrderBib.size(); i++) { f += gOrderBib[i]; f += '\n'; }   // (reordenar tambien es un cambio)
    f += '\x01';
    std::vector<std::string> rows;
    for (size_t i = 0; i < its.size(); i++)
        if (!its[i].folder.empty()) rows.push_back(W3dBibKey(its[i].type, its[i].id) + "\x02" + its[i].folder);
    std::sort(rows.begin(), rows.end());
    for (size_t i = 0; i < rows.size(); i++) { f += rows[i]; f += '\n'; }
    return f;
}

// ---- EL UNDO DE LAS CARPETAS: un paso = el estado ANTES y DESPUES (las carpetas creadas + la
//      carpeta de cada item que cambio, de cualquier tipo). Aplicar = ir al otro. ----
struct StepFolders {
    std::vector<std::string> createdPrev, createdAfter;
    std::vector<std::string> orderPrev, orderAfter;   // el orden manual
    std::vector<int> types;
    std::vector<std::string> ids, prev, after;
    bool inPrev;
    StepFolders() : inPrev(false) {}
};
static void StepFoldersGo(StepFolders* p, bool aPrev) {
    gCreated = aPrev ? p->createdPrev : p->createdAfter;
    gOrderBib = aPrev ? p->orderPrev : p->orderAfter; OrderChange();
    for (size_t i = 0; i < p->ids.size(); i++) {
        W3dResourceProvider* prov = W3dResourcesProviderView(p->types[i]);
        if (prov) prov->SetFolder(p->ids[i], aPrev ? p->prev[i] : p->after[i]);
    }
    p->inPrev = aPrev;
}
static void StepFoldersApply(void* d) {
    StepFolders* p = (StepFolders*)d;
    StepFoldersGo(p, !p->inPrev);
}
static void StepFoldersFree(void* d) { delete (StepFolders*)d; }

// el GRUPO abierto (W3dViewRecGroupStart): mientras dure, las operaciones no arman su propio
// paso; el grupo toma UNA foto al abrir y confirma UN paso al cerrar
static int gGroupProf = 0;

// una operacion de carpetas: foto antes, foto despues, y si algo cambio UN paso de undo
class TransactionFolders {
public:
    bool active;   // false = adentro de un grupo (el paso lo arma el grupo)
    std::vector<std::string> created0, order0;
    std::map<std::string, std::string> items0;   // clave de biblioteca -> carpeta
    explicit TransactionFolders(bool isGroup = false) : active(true) {
        if (!isGroup && gGroupProf > 0) { active = false; return; }
        created0 = gCreated;
        order0 = gOrderBib;
        std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
        for (size_t i = 0; i < its.size(); i++) items0[W3dBibKey(its[i].type, its[i].id)] = its[i].folder;
    }
    // el paso con lo que cambio desde la foto (NULL = nada)
    StepFolders* Step() {
        StepFolders* p = new StepFolders();
        p->createdPrev = created0;
        p->createdAfter = gCreated;
        p->orderPrev = order0;
        p->orderAfter = gOrderBib;
        std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
        for (size_t i = 0; i < its.size(); i++) {
            std::map<std::string, std::string>::iterator a = items0.find(W3dBibKey(its[i].type, its[i].id));
            if (a == items0.end() || a->second == its[i].folder) continue;
            p->types.push_back(its[i].type); p->ids.push_back(its[i].id);
            p->prev.push_back(a->second); p->after.push_back(its[i].folder);
        }
        if (p->ids.empty() && p->createdPrev == p->createdAfter && p->orderPrev == p->orderAfter) { delete p; return NULL; }
        return p;
    }
    bool Confirm() {
        if (!active) return false;
        StepFolders* p = Step();
        if (!p) return false;
        UndoExternal f; f.apply = StepFoldersApply; f.free = StepFoldersFree;
        NoRoot sr;
        UndoPushExternal(f, p);
        return true;
    }
    // vuelve todo a la foto sin dejar paso (el Esc del modo mover)
    void Cancel() {
        StepFolders* p = Step();
        if (!p) return;
        StepFoldersGo(p, true);
        delete p;
    }
};

void W3dBibOrderSet(const std::vector<std::string>& siblings) {
    if (siblings.empty()) return;
    TransactionFolders tx;
    std::set<std::string> van(siblings.begin(), siblings.end());
    std::vector<std::string> newValue;
    for (size_t i = 0; i < gOrderBib.size(); i++) if (!van.count(gOrderBib[i])) newValue.push_back(gOrderBib[i]);
    newValue.insert(newValue.end(), siblings.begin(), siblings.end());
    gOrderBib.swap(newValue);
    OrderChange();
    tx.Confirm();
}

// los nombres de las hermanas de una carpeta nueva (las carpetas hijas directas de 'padre')
static void Siblings(const std::string& parent, std::vector<std::string>& out) {
    out.clear();
    std::vector<std::string> all; W3dFoldersAll(all);
    for (size_t i = 0; i < all.size(); i++)
        if (W3dFolderParent(all[i]) == parent) out.push_back(W3dFolderLeaf(all[i]));
}

bool W3dNewFolder(const std::string& parent0, const std::string& name, std::string* path) {
    const std::string parent = W3dFolderNormalize(parent0);
    std::string leaf = W3dFolderNormalize(name);
    // el nombre es UN tramo: la barra no arma subcarpetas desde aca (para eso se crea adentro)
    for (size_t i = 0; i < leaf.size(); i++) if (leaf[i] == '/') leaf[i] = '-';
    std::vector<std::string> hs; Siblings(parent, hs);
    leaf = W3dUniqueNameInValues(leaf.empty() ? std::string("New Folder") : leaf, "New Folder", hs, -1);
    const std::string r = parent.empty() ? leaf : parent + "/" + leaf;
    TransactionFolders tx;
    CreatedAdd(r);
    tx.Confirm();
    if (path) *path = r;
    return true;
}

// la carpeta 'ruta' pasa a llamarse 'nueva' (otra hoja o/y otra carpeta de arriba): sus subcarpetas
// creadas y sus items (los que no son de solo lectura, de todos los tipos) cambian de prefijo. Sin
// undo (lo arma quien llama).
static void RelocateFolder(const std::string& path, const std::string& newValue) {
    std::vector<std::string> cr = gCreated;
    gCreated.clear();
    bool was = false;
    for (size_t i = 0; i < cr.size(); i++) {
        if (W3dFolderInside(cr[i], path)) { CreatedAdd(newValue + cr[i].substr(path.size())); was = true; }
        else CreatedAdd(cr[i]);
    }
    if (!was) CreatedAdd(newValue);   // era implicita (solo por sus items): queda creada
    std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
    for (size_t i = 0; i < its.size(); i++) {
        if (its[i].readOnly || !its[i].parent.empty() || !W3dFolderInside(its[i].folder, path)) continue;
        W3dResourceProvider* prov = W3dResourcesProviderView(its[i].type);
        if (prov) prov->SetFolder(its[i].id, newValue + its[i].folder.substr(path.size()));
    }
}

// la carpeta EXISTE (creada, o de algun recurso). "" (la raiz) siempre.
static bool FolderExists(const std::string& path) {
    if (path.empty()) return true;
    std::vector<std::string> all; W3dFoldersAll(all);
    return std::find(all.begin(), all.end(), path) != all.end();
}

bool W3dFolderRename(const std::string& path0, const std::string& name, std::string* newPath) {
    const std::string path = W3dFolderNormalize(path0);
    if (path.empty() || !FolderExists(path)) return false;
    std::string leaf = W3dFolderNormalize(name);
    for (size_t i = 0; i < leaf.size(); i++) if (leaf[i] == '/') leaf[i] = '-';
    if (leaf.empty()) { if (newPath) *newPath = path; return false; }
    const std::string parent = W3dFolderParent(path);
    // unico entre las hermanas (sin contarse a si misma)
    std::vector<std::string> hs; Siblings(parent, hs);
    const std::string old = W3dFolderLeaf(path);
    for (size_t i = 0; i < hs.size(); i++) if (hs[i] == old) { hs.erase(hs.begin() + (long)i); break; }
    leaf = W3dUniqueNameInValues(leaf, "New Folder", hs, -1);
    const std::string newValue = parent.empty() ? leaf : parent + "/" + leaf;
    if (newPath) *newPath = newValue;
    if (newValue == path) return true;
    TransactionFolders tx;
    RelocateFolder(path, newValue);
    tx.Confirm();
    return true;
}

bool W3dFolderEmpty(const std::string& path0) {
    const std::string path = W3dFolderNormalize(path0);
    std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
    for (size_t i = 0; i < its.size(); i++)
        if (W3dFolderInside(its[i].folder, path)) return false;
    return true;
}

bool W3dFolderDelete(const std::string& path0, std::string* reason) {
    const std::string path = W3dFolderNormalize(path0);
    if (path.empty()) return false;
    if (!FolderExists(path)) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (!W3dFolderEmpty(path)) { if (reason) *reason = "The folder isn't empty"; return false; }
    TransactionFolders tx;
    std::vector<std::string> cr = gCreated;
    gCreated.clear();
    for (size_t i = 0; i < cr.size(); i++)
        if (!W3dFolderInside(cr[i], path)) CreatedAdd(cr[i]);
    // la carpeta de ARRIBA sigue existiendo aunque quede vacia (se borra una sola por vez)
    CreatedAdd(W3dFolderParent(path));
    tx.Confirm();
    return true;
}

bool W3dFolderMove(const std::string& path0, const std::string& destination0, std::string* newPath) {
    const std::string path = W3dFolderNormalize(path0), destination = W3dFolderNormalize(destination0);
    if (newPath) *newPath = path;
    if (path.empty()) return false;
    // adentro de si misma (o de una de sus subcarpetas): no hay donde ponerla
    if (W3dFolderInside(destination, path)) return false;
    if (!FolderExists(path) || !FolderExists(destination)) return false;
    if (W3dFolderParent(path) == destination) return true;   // ya esta ahi
    // su nombre, hecho unico entre las carpetas que ya cuelgan del destino
    std::vector<std::string> hs; Siblings(destination, hs);
    const std::string leaf = W3dUniqueNameInValues(W3dFolderLeaf(path), "New Folder", hs, -1);
    const std::string newValue = destination.empty() ? leaf : destination + "/" + leaf;
    if (newPath) *newPath = newValue;
    TransactionFolders tx;
    RelocateFolder(path, newValue);
    // la de ARRIBA de la que sale sigue existiendo aunque quede vacia (como al mover un recurso)
    CreatedAdd(W3dFolderParent(path));
    CreatedAdd(destination);
    tx.Confirm();
    return true;
}

// el GRUPO: una foto al abrir (la transaccion de afuera) y un paso al cerrar
static TransactionFolders* gGroupTx = NULL;
void W3dViewRecGroupStart() {
    if (gGroupProf++ > 0) return;                 // anidado: manda el de afuera
    delete gGroupTx;
    gGroupTx = new TransactionFolders(true);
}
void W3dViewRecGroupEnd() {
    if (gGroupProf <= 0) return;
    if (--gGroupProf > 0) return;
    TransactionFolders* tx = gGroupTx;
    gGroupTx = NULL;
    if (tx) { tx->Confirm(); delete tx; }
}
void W3dViewRecGroupCancel() {
    if (gGroupProf <= 0) return;
    gGroupProf = 0;
    TransactionFolders* tx = gGroupTx;
    gGroupTx = NULL;
    if (tx) { tx->Cancel(); delete tx; }
}
void (*W3dViewRecGroupCancelHook)() = 0;
bool W3dViewRecBeforeUndo() {
    if (gGroupProf <= 0) return false;
    if (W3dViewRecGroupCancelHook) W3dViewRecGroupCancelHook();   // el outliner sale de su modo mover
    W3dViewRecGroupCancel();   // (si nadie lo cerro: todo vuelve a la foto)
    w3dLogf("[biblioteca] Ctrl+Z en el modo mover: se cancelo el modo (nada mas se deshizo)");
    return true;
}

// ============================================================================
//  ACCIONES SOBRE UN ITEM
// ============================================================================
bool W3dViewRecMove(int view, const std::string& id, const std::string& folder0) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov) return false;
    W3dResourceItem it;
    if (!W3dViewRecInfo(view, id, &it) || it.readOnly || !it.parent.empty()) return false;   // (un hijo va con su padre)
    const std::string folder = W3dFolderNormalize(folder0);
    if (it.folder == folder) return true;
    TransactionFolders tx;
    if (!prov->SetFolder(id, folder)) return false;
    // la carpeta de la que sale sigue existiendo aunque quede vacia (se borra a mano)
    CreatedAdd(it.folder);
    CreatedAdd(folder);
    tx.Confirm();
    return true;
}

// renombrar o borrar un recurso del PROYECTO (malla, material, textura, animset, archivo, PREFAB) con escenas
// o prefabs sin abrir: se cargan antes, asi sus objetos siguen al nuevo nombre (o lo retienen como
// usuarios). Las referencias van por NOMBRE: una escena sin cargar quedaria nombrando lo que ya no
// existe (ver W3dRoots.h). Un PREFAB tambien: sus usuarios son sus instancias, y las de una escena sin
// abrir no se contaban (el prefab parecia huerfano, se borraba sin aviso y al abrir esa escena su instancia
// quedaba vacia). Las escenas no lo necesitan.
static void LoadRootsIfNeeded(int view) {
    if (view == W3D_VIEW_SCENES) return;
    if (W3dRootsLoadAll() > 0) gScriptsRead = false;
}

bool W3dViewRecRename(int view, const std::string& id, const std::string& newValue, std::string* final) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov) return false;
    LoadRootsIfNeeded(view);
    W3dResourceItem it;
    if (!W3dViewRecInfo(view, id, &it) || it.readOnly || !it.renamable) return false;
    // EL MISMO NOMBRE QUE SE VE = no se toca nada. Los dos renames (en linea y el Name de Properties)
    // confirman al perder el foco, y el Esc solo devuelve el campo al texto de antes: sin esto, un
    // nombre que no esta en la forma que pide su tipo (una textura "textures/Piso_A.png", que como
    // entrada pediria "piso_a.png"; un material con blancos en los bordes) se renombraba solo, con
    // su paso de undo, justo cuando el usuario pidio cancelar.
    if (newValue == it.name) { if (final) *final = id; return true; }
    std::string remained;
    NoRoot sr;   // (el nombre de un recurso es de la biblioteca: la raiz activa no se edito)
    const std::string request = prov->IdRequest(id, newValue);   // (antes: el renombre cambia lo ocupado)
    if (!prov->Rename(id, newValue, &remained)) return false;
    if (remained != request) W3dNoticeAlreadyExists(request, remained);   // informativo: el nombre ya estaba
    if (final) *final = remained;
    W3dActiveRenamedResource(view, id, remained);
    return true;
}

bool W3dViewRecDelete(int view, const std::string& id, std::string* reason) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov) return false;
    LoadRootsIfNeeded(view);
    // la foto de los scripts puede estar vieja: se re-escanea ANTES de decidir que es huerfano
    gScriptsRead = false;
    W3dResourceItem it;
    if (!W3dViewRecInfo(view, id, &it)) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (it.readOnly) { if (reason) *reason = "It is read-only"; return false; }
    // (una PARTE de un recurso -un clip de su biblioteca- se borra aunque el recurso este en uso: el proveedor
    // lo hace con su paso de undo)
    if (it.users > 0 && it.parent.empty()) { if (reason) *reason = "It is in use"; return false; }
    if (!prov->Delete(id, reason)) return false;
    int v = -1; std::string a;
    if (W3dResourceActive(&v, &a) && v == view && a == id) W3dResourceDeactivate();
    return true;
}

bool W3dViewRecKnowsDeleteInUse(int view) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    return prov && prov->KnowsDeleteInUse();
}
bool W3dViewRecDeleteForced(int view, const std::string& id, std::string* reason) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov) return false;
    LoadRootsIfNeeded(view);
    gScriptsRead = false;
    W3dResourceItem it;
    if (!W3dViewRecInfo(view, id, &it)) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (it.readOnly) { if (reason) *reason = "It is read-only"; return false; }
    if (it.users == 0 || !it.parent.empty()) return W3dViewRecDelete(view, id, reason);
    if (!prov->KnowsDeleteInUse()) { if (reason) *reason = "It is in use"; return false; }
    if (!prov->DeleteInUse(id, reason)) return false;
    int v = -1; std::string a;
    if (W3dResourceActive(&v, &a) && v == view && a == id) W3dResourceDeactivate();
    W3dResourcesViewInvalidate();
    return true;
}

int W3dViewRecPurge(int view, std::vector<std::string>* names) {
    if (names) names->clear();
    if (view < 0) {
        // TODOS los tipos (la biblioteca sin filtro)
        std::vector<int> types; W3dLibraryTypes(types);
        int n = 0;
        for (size_t i = 0; i < types.size(); i++) {
            std::vector<std::string> ns;
            n += W3dViewRecPurge(types[i], &ns);
            if (names) names->insert(names->end(), ns.begin(), ns.end());
        }
        return n;
    }
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov || !prov->Purgeable()) return 0;   // (escenas/prefabs: se borran de a uno, a pedido)
    LoadRootsIfNeeded(view);
    gScriptsRead = false;
    std::vector<W3dResourceItem> its; W3dViewRecList(view, its);
    int n = 0;
    for (size_t i = 0; i < its.size(); i++) {
        if (its[i].users > 0 || its[i].readOnly || !its[i].parent.empty()) continue;   // (las partes no se purgan)
        std::string reason;
        if (!W3dViewRecDelete(view, its[i].id, &reason)) {
            w3dLogf("[biblioteca] no se purgo '%s': %s", its[i].name.c_str(), reason.c_str());
            continue;
        }
        n++;
        if (names) names->push_back(its[i].name);
    }
    return n;
}

int W3dViewRecSelectUsers(int view, const std::string& id) {
    std::vector<std::string> ids(1, id);
    return W3dViewRecSelectUsers(view, ids);
}
// la UNION de los OBJETOS de la escena que usan los recursos (un objeto que usa dos va una vez).
// 'claves' = claves de biblioteca (W3dBibKey)
static void UnionUsers(const std::vector<std::string>& keys, std::vector<Object*>& us) {
    us.clear();
    std::set<Object*> seen;
    for (size_t k = 0; k < keys.size(); k++) {
        int t = 0; std::string id;
        if (!W3dBibByKey(keys[k], &t, &id)) continue;
        W3dResourceProvider* prov = W3dResourcesProviderView(t);
        if (!prov) continue;
        std::vector<Object*> one;
        prov->Users(id, one);
        // solo los de la escena ACTIVA se pueden seleccionar (los de otra escena o prefab cargado
        // cuentan como usuarios, pero no estan en el arbol que se ve)
        for (size_t i = 0; i < one.size(); i++)
            if (one[i] && W3dRootOf(one[i]) == SceneCollection && seen.insert(one[i]).second) us.push_back(one[i]);
    }
}
static std::vector<std::string> KeysOf(int view, const std::vector<std::string>& ids) {
    std::vector<std::string> c;
    for (size_t i = 0; i < ids.size(); i++) c.push_back(W3dBibKey(view, ids[i]));
    return c;
}
int W3dBibObjectsUsers(const std::vector<std::string>& keys) {
    std::vector<Object*> us;
    UnionUsers(keys, us);
    return (int)us.size();
}
int W3dViewRecObjectsUsers(int view, const std::vector<std::string>& ids) {
    return W3dBibObjectsUsers(KeysOf(view, ids));
}
int W3dBibSelectUsers(const std::vector<std::string>& keys) {
    std::vector<Object*> us;
    UnionUsers(keys, us);
    // sin OBJETOS que seleccionar (lo usan una malla huerfana del registro, un script, un flipbook,
    // otro material...): la seleccion del usuario NO se toca
    if (us.empty()) return 0;
    UndoCaptureSelection();
    DeselectAll();
    for (size_t i = 0; i < us.size(); i++) {
        us[i]->Select();
        // que se vean en el arbol: los padres se despliegan
        for (Object* p = us[i]->Parent; p && p != SceneCollection; p = p->Parent) p->expanded = true;
    }
    return (int)us.size();
}
int W3dViewRecSelectUsers(int view, const std::vector<std::string>& ids) {
    return W3dBibSelectUsers(KeysOf(view, ids));
}

bool W3dViewRecDuplicate(int view, const std::string& id, std::string* idNew, std::string* reason) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov) { if (reason) *reason = "This resource type can't be duplicated"; return false; }
    W3dResourceItem it;
    if (!W3dViewRecInfo(view, id, &it)) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    NoRoot sr;
    if (!prov->Duplicate(id, idNew, reason)) return false;
    // la copia va a la MISMA carpeta
    if (idNew && !it.folder.empty()) prov->SetFolder(*idNew, it.folder);
    W3dResourcesViewInvalidate();
    return true;
}
void W3dViewRecData(int view, const std::string& id, std::vector<W3dResourceData>& out) {
    out.clear();
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (prov) prov->Data(id, out);
}
int W3dViewRecLocation(int view, const std::string& id) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    return prov ? prov->Location(id) : -1;
}
std::string W3dViewRecPathExternalSuggested(int view, const std::string& id) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    return prov ? prov->PathExternalSuggested(id) : std::string();
}
bool W3dViewRecSetLocation(int view, const std::string& id, int location, const std::string& externalPath,
                               std::string* idNew, std::string* reason) {
    W3dResourceProvider* prov = W3dResourcesProviderView(view);
    if (!prov) return false;
    NoRoot sr;
    const bool ok = prov->SetLocation(id, location, externalPath, idNew, reason);
    if (ok) W3dResourcesViewInvalidate();
    return ok;
}

// ============================================================================
//  EL RECURSO ACTIVO
// ============================================================================
static int          gActView = -1;
static std::string  gActId;
static unsigned int gActObjSerial = 0;   // el objeto activo cuando se eligio el recurso
// W3dSelectionSerial cuando se eligio: si la escena ELIGE un objeto despues, el recurso se suelta aunque el
// objeto activo sea el mismo (un recurso elegido en Edit Mode, con la malla en edicion todavia activa)
static unsigned int gActSelSerial = 0;
static unsigned     gActVersion = 0;
static Texture*     gPrevTex = NULL;     // la vista previa que se CARGO para el activo (se suelta)
static std::string  gFailedPrev;         // la clave que no cargo (no reintentar por cuadro)

// algun material (vivo, purgado o animado) guarda ese puntero
static bool TextureInMaterials(const Texture* t) {
    for (size_t i = 0; i < Materials.size(); i++) if (MaterialNamesTexture(Materials[i], t)) return true;
    const std::vector<Material*>& mp = MaterialsPurged();
    for (size_t i = 0; i < mp.size(); i++) if (MaterialNamesTexture(mp[i], t)) return true;
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                if (AnimatedMaterials[i]->frameTextures[f] == t) return true;
    return false;
}
static void ReleasePreviewView() {
    if (gPrevTex) {
        // sigue siendo del cache? (la pudo liberar el cierre del proyecto antes de llegar aca)
        bool live = false;
        for (size_t i = 0; i < Textures.size() && !live; i++) live = (Textures[i] == gPrevTex);
        if (live) {
            // la referencia de la VISTA PREVIA es suya: se suelta. Si alguien mas la tiene (la ranura de un material
            // que la tomo mientras se miraba, otro pedido) solo baja una; si era la unica y nadie la nombra, sale de la
            // GPU al cementerio (el objeto queda para cualquier puntero viejo). Si era la unica pero una ranura la nombra
            // sin referencia propia (un camino viejo), esa referencia pasa a ser de la ranura
            W3dResource* r = W3dResourceFind(W3DREC_TEXTURE, gPrevTex->path);
            const int refs = (r && r->data == gPrevTex) ? r->refTotal : 0;
            if (refs > 1) TextureRelease(gPrevTex);
            else if (TextureInMaterials(gPrevTex)) { /* la ranura se queda con ella */ }
            else TexturePurge(gPrevTex);
        }
    }
    gPrevTex = NULL;
    gFailedPrev.clear();
}
// la vista previa del activo ES esa textura (la cargo solo el panel para mirarla): se suelta
static void ReleasePreviewViewOf(const std::string& key) {
    if (gPrevTex && KeyTexture(gPrevTex->path) == key) ReleasePreviewView();
}
// las entradas cambiaron de nombre: la que no cargo puede cargar ahora
static void ForgetFailedPreviewView() { gFailedPrev.clear(); }
extern int W3dLibsCount();   // (io/Libraries.h)
static int gExtLib = -1;            // el recurso de una LIBRERIA elegido (W3dResourceExtActivate): -1 = ninguno
static W3dResourceItem gExtItem;
static std::string gExtGlobal;
static unsigned int gExtObjSerial = 0, gExtSelSerial = 0;
void W3dResourceExtActivate(int lib, const W3dResourceItem& it, const std::string& idGlobal) {
    W3dResourceDeactivate();
    gExtLib = lib; gExtItem = it; gExtGlobal = idGlobal;
    gExtObjSerial = ActiveObj ? ActiveObj->serial : 0;
    gExtSelSerial = W3dSelectionSerial;
    gActVersion++;
}
bool W3dResourceExtActive(int* lib, W3dResourceItem* it, std::string* idGlobal) {
    if (gExtLib < 0) return false;
    if ((ActiveObj ? ActiveObj->serial : 0) != gExtObjSerial || W3dSelectionSerial != gExtSelSerial || gExtLib >= W3dLibsCount()) {
        gExtLib = -1; gActVersion++; return false;
    }
    if (lib) *lib = gExtLib;
    if (it) *it = gExtItem;
    if (idGlobal) *idGlobal = gExtGlobal;
    return true;
}
void W3dResourceActivate(int view, const std::string& id) {
    if (gExtLib >= 0) { gExtLib = -1; gActVersion++; }
    if (view != gActView || id != gActId) { ReleasePreviewView(); gActVersion++; }
    gActView = view;
    gActId = id;
    gActObjSerial = ActiveObj ? ActiveObj->serial : 0;
    gActSelSerial = W3dSelectionSerial;
}
void W3dResourceDeactivate() {
    if (gActView >= 0 || gExtLib >= 0) gActVersion++;
    gExtLib = -1;
    ReleasePreviewView();
    gActView = -1;
    gActId.clear();
}
unsigned W3dActiveResourceVersion() { return gActVersion; }
bool W3dResourceActive(int* view, std::string* id) {
    if (gActView < 0) return false;
    // el usuario volvio a la escena (eligio un objeto, aunque sea el que seguia activo en Edit Mode, u
    // otro objeto quedo activo): el panel vuelve a mostrar objetos
    if ((ActiveObj ? ActiveObj->serial : 0) != gActObjSerial || W3dSelectionSerial != gActSelSerial) {
        W3dResourceDeactivate();
        return false;
    }
    if (!W3dViewRecInfo(gActView, gActId, NULL)) { W3dResourceDeactivate(); return false; }
    if (view) *view = gActView;
    if (id) *id = gActId;
    return true;
}
void W3dActiveRenamedResource(int view, const std::string& old, const std::string& newValue) {
    if (view == gActView && old == gActId && old != newValue) { gActId = newValue; gActVersion++; }
}
Material* W3dActiveResourceMaterial() {
    int v; std::string id;
    if (!W3dResourceActive(&v, &id) || v != W3D_VIEW_MATERIALS) return NULL;
    return MaterialByName(id);
}
MeshResource* W3dActiveResourceMesh() {
    int v; std::string id;
    if (!W3dResourceActive(&v, &id) || v != W3D_VIEW_MESHES) return NULL;
    return W3dMeshResourceByName(id);
}
Texture* W3dLoadedTextureOf(const std::string& id) { return LoadedTextureOf(KeyTexture(id)); }
std::string W3dTextureKey(const std::string& path) { return KeyTexture(path); }
Material* W3dMaterialById(const std::string& id) { return MaterialByName(id); }
Texture* W3dActiveResourceTexture() {
    int v; std::string id;
    if (!W3dResourceActive(&v, &id) || v != W3D_VIEW_TEXTURES) return NULL;
    for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++)
        if (Textures[i] && KeyTexture(Textures[i]->path) == id) return Textures[i];
    if (gPrevTex) return gPrevTex;
    if (gFailedPrev == id) return NULL;
    // la vista previa de antes de esa misma entrada quedo en el cementerio: vuelve (mismo objeto)
    const std::vector<Texture*>& tp = TexturesPurged();
    for (size_t i = 0; i < tp.size(); i++)
        if (tp[i] && KeyTexture(tp[i]->path) == id && TextureRevive(tp[i])) {
            gPrevTex = tp[i];
            // (la de la vista previa + la de cada paso de undo que la guarda: ver SlotsOf)
            for (int k = W3dUndoTextureOwners(gPrevTex); k > 0; k--) TextureRetain(gPrevTex);
            return gPrevTex;
        }
    // no esta cargada (una entrada huerfana): se toma UNA referencia para la vista previa
    gPrevTex = TextureTake(id);
    if (!gPrevTex) gFailedPrev = id;
    return gPrevTex;
}

// ============================================================================
//  GUARDADO / CARGA / CIERRE
// ============================================================================
void W3dResourcesViewSaveBefore() {
    gTexPrev.clear();
    for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (!t || t->path.empty()) continue;
        const std::string k = KeyTexture(t->path);
        if (gFolderTex.count(k)) gTexPrev.push_back(std::make_pair(t, k));
    }
}

// un string JSON (el mismo escape que el resto del guardado)
static void JEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const unsigned char c = (unsigned char)v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += (char)c; }
        else if (c == '\n') s += "\\n";
        else if (c == '\r') s += "\\r";
        else if (c == '\t') s += "\\t";
        else if (c < 0x20) { char b[8]; sprintf(b, "\\u%04x", (unsigned)c); s += b; }
        else s += (char)c;
    }
    s += '"';
}
static void JsonTextList(std::string& s, const std::vector<std::string>& v) {
    s += "[";
    for (size_t i = 0; i < v.size(); i++) { if (i) s += ", "; JEsc(s, W3dCosmeticPathJson(v[i])); }
    s += "]";
}

void W3dResourcesViewSaveJson(std::string& s, W3dContainerWriter* esc) {
    // LA CARPETA SIGUE A SU TEXTURA: la ingesta del guardado cambio la ruta de disco por el
    // nombre de entrada ("/home/.../piso.png" -> "textures/piso.png")
    for (size_t i = 0; i < gTexPrev.size(); i++) {
        Texture* t = gTexPrev[i].first;
        bool live = false;
        for (size_t k = 0; k < Textures.size() && !live; k++) live = (Textures[k] == t);
        if (!live) continue;
        const std::string newValue = KeyTexture(t->path);
        if (newValue == gTexPrev[i].second) continue;
        std::map<std::string, std::string>::iterator c = gFolderTex.find(gTexPrev[i].second);
        if (c == gFolderTex.end()) continue;
        const std::string folder = c->second;
        gFolderTex.erase(c);
        gFolderTex[newValue] = folder;
    }
    gTexPrev.clear();
    // ...y la de disco que NO esta en Textures (la usan solo particulas, elementos 2D, flipbooks):
    // su ruta la reescribio la ingesta de ESE objeto, no la de un material. El escritor sabe a que
    // entrada fue cada ruta de disco que le llego: la carpeta sigue a esa entrada.
    if (esc) {
        std::vector<std::pair<std::string, std::string> > ing;
        esc->Ingested(ing);
        std::map<std::string, std::string> aEntry;   // clave de disco -> clave de su entrada
        for (size_t i = 0; i < ing.size(); i++) aEntry[KeyTexture(ing[i].first)] = KeyTexture(ing[i].second);
        std::vector<std::pair<std::string, std::string> > move;   // (clave vieja, clave nueva)
        for (std::map<std::string, std::string>::iterator it = gFolderTex.begin(); it != gFolderTex.end(); ++it) {
            if (it->second.empty() || W3dIsEntryName(it->first)) continue;
            std::map<std::string, std::string>::iterator e = aEntry.find(it->first);
            if (e != aEntry.end() && e->second != it->first) move.push_back(std::make_pair(it->first, e->second));
        }
        for (size_t i = 0; i < move.size(); i++) {
            const std::string folder = gFolderTex[move[i].first];
            gFolderTex.erase(move[i].first);
            // (si la entrada ya tenia la suya, manda esa: es la que el usuario ve en la vista)
            if (!gFolderTex.count(move[i].second)) gFolderTex[move[i].second] = folder;
        }
    }
    // "carpetas": el ARBOL UNICO de las carpetas creadas (solo si hay alguna)
    if (!gCreated.empty()) {
        s += "  \"folders\": ";
        JsonTextList(s, gCreated);
        s += ",\n";
    }
    // "orden": el orden MANUAL de la biblioteca (lo que se reordeno arrastrando), solo lo que sigue existiendo
    {
        std::vector<std::string> liveSet;
        for (size_t i = 0; i < gOrderBib.size(); i++) {
            int t = 0; std::string id; W3dResourceItem it;
            if (W3dBibByKey(gOrderBib[i], &t, &id) && W3dViewRecInfo(t, id, &it)) liveSet.push_back(gOrderBib[i]);
        }
        if (!liveSet.empty()) {
            s += "  \"order\": ";
            JsonTextList(s, liveSet);
            s += ",\n";
        }
    }
    // "texturas": la carpeta de cada textura que la tiene. Solo las que van a estar ADENTRO del
    // archivo: la verificacion del guardado exige que toda entrada nombrada exista.
    W3dZipReader* old = W3dContainerReader();
    std::vector<std::pair<std::string, std::string> > rows;
    for (std::map<std::string, std::string>::iterator it = gFolderTex.begin(); it != gFolderTex.end(); ++it) {
        if (it->second.empty() || !W3dIsEntryName(it->first) || gPurgedTex.count(it->first)) continue;
        // (la escrita por este guardado, la del contenedor viejo, o la importada en la sesion que
        // PreserveTransient conserva)
        const bool va = (esc && esc->Has(it->first)) || (old && old->Exists(it->first)) ||
                        W3dContainerEntryEdited(it->first);
        if (va) rows.push_back(*it);
    }
    if (!rows.empty()) {
        s += "  \"textures\": [\n";
        for (size_t i = 0; i < rows.size(); i++) {
            s += "    { \"entry\": "; JEsc(s, rows[i].first);
            s += ", \"folder\": "; JEsc(s, W3dCosmeticPathJson(rows[i].second)); s += " }";
            s += (i + 1 < rows.size()) ? ",\n" : "\n";
        }
        s += "  ],\n";
    }
    // "archivos": la carpeta de cada sonido / script / fuente / video que la tiene (las entradas que van a
    // estar ADENTRO; los de afuera se nombran por su ruta de disco y el guardado los reescribe "ext:")
    std::vector<std::pair<std::string, std::string> > fa;
    for (std::map<std::string, std::string>::iterator it = gFolderArch.begin(); it != gFolderArch.end(); ++it) {
        if (it->second.empty() || !W3dIsEntryName(it->first) || gPurgedArch.count(it->first)) continue;
        const bool va = (esc && esc->Has(it->first)) || (old && old->Exists(it->first)) ||
                        W3dContainerEntryEdited(it->first);
        if (va) fa.push_back(*it);
    }
    if (!fa.empty()) {
        s += "  \"files\": [\n";
        for (size_t i = 0; i < fa.size(); i++) {
            s += "    { \"entry\": "; JEsc(s, fa[i].first);
            s += ", \"folder\": "; JEsc(s, W3dCosmeticPathJson(fa[i].second)); s += " }";
            s += (i + 1 < fa.size()) ? ",\n" : "\n";
        }
        s += "  ],\n";
    }
    // "externos": lo de la biblioteca que vive AFUERA del .w3d a proposito ("ext:", como cualquier referencia
    // externa: relativa a la carpeta del .w3d si esta abajo), con su carpeta. Los que alguien usa ya van
    // nombrados por su usuario (aca solo si tienen carpeta: "texturas"/"archivos" llevan solo las de adentro);
    // un HUERFANO de afuera no lo nombra nadie y sin esta lista se perdia al volver a abrir. Pasan por la
    // ingesta: quedan en EXTERNALS.txt (y en el aviso del guardado si el archivo falta).
    if (esc) {
        static const int kTypes[6] = { W3D_VIEW_TEXTURES, W3D_VIEW_SOUNDS, W3D_VIEW_SCRIPTS, W3D_VIEW_FONTS, W3D_VIEW_VIDEOS, W3D_VIEW_SHADERS };
        std::string ex;
        for (int k = 0; k < 6; k++) {
            std::vector<W3dResourceItem> its;
            W3dViewRecList(kTypes[k], its);
            const std::set<std::string>& pur = PurgedOf(kTypes[k]);
            for (size_t i = 0; i < its.size(); i++) {
                const W3dResourceItem& it = its[i];
                if (LocationFromPath(it.id) != 1 || pur.count(it.id)) continue;
                if (it.users > 0 && it.folder.empty()) continue;
                std::string path = ExternalPathMarked(it.id);
                if (path.empty()) continue;   // (una ruta de disco que no se eligio afuera: no es de aca)
                const std::string ref = esc->Ingest(path, W3dCategoryByExtension(path), "biblioteca \"" + it.name + "\"");
                if (ref.compare(0, 4, "ext:") != 0) continue;
                ex += ex.empty() ? "" : ",\n";
                ex += "    { \"type\": "; JEsc(ex, W3dViewKey(kTypes[k]));
                ex += ", \"file\": "; JEsc(ex, ref);
                if (!it.folder.empty()) { ex += ", \"folder\": "; JEsc(ex, W3dCosmeticPathJson(it.folder)); }
                ex += " }";
            }
        }
        if (!ex.empty()) s += "  \"external\": [\n" + ex + "\n  ],\n";
    }
}

void W3dResourcesViewSaveInMemory(W3dContainerWriter* esc) {
    if (!esc) return;
    for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (!t || t->path.empty() || !W3dIsEntryName(t->path)) continue;
        // (la escribio un usuario que la nombra, o se purgo: no es de este camino)
        if (esc->Has(t->path) || gPurgedTex.count(KeyTexture(t->path))) continue;
        std::string png;
        if (!TexEditBytesToSave(t->path, png)) continue;   // no vive solo en memoria
        if (esc->AddBytes(t->path, png, true))
            w3dLogf("[outliner] textura huerfana en memoria guardada: %s (%u bytes)", t->path.c_str(), (unsigned)png.size());
    }
}

void W3dResourcesViewSaveDiscards(W3dContainerWriter* esc) {
    if (!esc) return;
    // LO QUE UN PASO DE UNDO PUEDE DEVOLVER (ver RetainedEntry): sus bytes se copian AHORA, con el contenedor
    // viejo todavia montado; despues del guardado esa entrada ya no existe
    for (std::set<RetainedEntry*>::iterator it = gRetained.begin(); it != gRetained.end(); ++it) {
        RetainedEntry* r = *it;
        if (!r->bytes.empty() || (!gPurgedTex.count(r->entry) && !gPurgedArch.count(r->entry))) continue;
        std::vector<unsigned char> d;
        if (W3dContainerExists(r->entry) && w3dFileSystem::ReadFileBytes(r->entry, d) && !d.empty()) r->bytes.swap(d);
    }
    for (std::set<std::string>::iterator it = gPurgedTex.begin(); it != gPurgedTex.end(); ++it)
        esc->Discard(*it);
    for (std::set<std::string>::iterator it = gPurgedArch.begin(); it != gPurgedArch.end(); ++it)
        esc->Discard(*it);
}

void W3dResourcesSavedView() {
    // lo purgado ya no esta en el archivo nuevo; el listado del contenedor cambio. Lo purgado de AFUERA
    // tampoco se escribio ("externos"): deja de ser de la biblioteca (lo que todavia nombra un objeto sigue
    // marcado y vuelve por su usuario)
    for (std::set<std::string>::iterator it = gPurgedTex.begin(); it != gPurgedTex.end(); ++it) gExternalTexBib.erase(*it);
    for (std::set<std::string>::iterator it = gPurgedArch.begin(); it != gPurgedArch.end(); ++it) {
        const int type = FileTypeFromPath(*it);
        if (type < 0 || LocationFromPath(*it) != 1) continue;
        std::vector<RefFile> refs; RefsFile(type, refs);
        bool named = false;
        for (size_t i = 0; i < refs.size() && !named; i++) named = (KeyTexture(*refs[i].path) == *it);
        if (!named) UnmarkExternal(*it);
    }
    gPurgedTex.clear();
    gPurgedArch.clear();
    W3dResourcesViewInvalidate();
}

// "ext:x" -> la ruta de disco, como la resuelve el lector del proyecto (JsonPath, import_w3d.cpp): relativa a
// la carpeta del .w3d; absoluta tal cual. "" = no es una referencia externa
static std::string ExternalPathOnOpen(const std::string& j) {
    if (j.size() <= 4 || j.compare(0, 4, "ext:") != 0) return std::string();
    const std::string x = j.substr(4);
    if (x[0] == '/' || (x.size() > 2 && x[1] == ':')) return x;
    return g_w3dDirProject.empty() ? x : g_w3dDirProject + "/" + x;
}

void W3dResourcesViewReadJson(JVal* root) {
    gCreated.clear();
    gOrderBib.clear(); OrderChange();
    gFolderTex.clear();
    gFolderArch.clear();
    gExternalTexBib.clear();
    if (!root) return;
    JVal* jc = JChild(root, "folders", 5);
    if (jc) {
        // el arbol UNICO (una lista de rutas)
        for (size_t i = 0; i < jc->list.size(); i++) {
            JVal* e = jc->list[i];
            if (e && e->type == 2) CreatedAdd(W3dFolderNormalize(e->str));
        }
    } else if ((jc = JChild(root, "folders", 4)) != NULL) {
        // MIGRACION: el formato de la fase anterior (un arbol POR TIPO: {"mallas": [...], "materiales": [...]})
        // se junta en el arbol unico. El proximo guardado escribe el formato nuevo.
        for (std::map<std::string, JVal*>::iterator it = jc->obj.begin(); it != jc->obj.end(); ++it) {
            if (!it->second || it->second->type != 5) continue;
            for (size_t i = 0; i < it->second->list.size(); i++) {
                JVal* e = it->second->list[i];
                if (e && e->type == 2) CreatedAdd(W3dFolderNormalize(e->str));
            }
        }
        w3dLogf("[biblioteca] carpetas por tipo migradas al arbol unico (%d)", (int)gCreated.size());
    }
    JVal* jo = JChild(root, "order", 5);
    if (jo)
        for (size_t i = 0; i < jo->list.size(); i++) {
            JVal* e = jo->list[i];
            if (e && e->type == 2 && W3dBibOrder(e->str) < 0) { gOrderBib.push_back(e->str); OrderChange(); }
        }
    JVal* jt = JChild(root, "textures", 5);
    if (jt)
        for (size_t i = 0; i < jt->list.size(); i++) {
            JVal* e = jt->list[i];
            if (!e || e->type != 4) continue;
            const std::string ent = JS(e, "entry", ""), ch = W3dFolderNormalize(JS(e, "folder", ""));
            if (!ent.empty() && !ch.empty()) gFolderTex[KeyTexture(ent)] = ch;
        }
    JVal* ja = JChild(root, "files", 5);
    if (ja)
        for (size_t i = 0; i < ja->list.size(); i++) {
            JVal* e = ja->list[i];
            if (!e || e->type != 4) continue;
            const std::string ent = JS(e, "entry", ""), ch = W3dFolderNormalize(JS(e, "folder", ""));
            if (!ent.empty() && !ch.empty()) gFolderArch[KeyTexture(ent)] = ch;
        }
    // "externos": lo de AFUERA que es de la biblioteca (queda marcado como cualquier referencia "ext:" al
    // abrir: el proximo guardado lo vuelve a escribir afuera), con su carpeta
    JVal* je = JChild(root, "external", 5);
    if (je)
        for (size_t i = 0; i < je->list.size(); i++) {
            JVal* e = je->list[i];
            if (!e || e->type != 4) continue;
            const int type = W3dViewByKey(JS(e, "type", ""));
            const std::string path = ExternalPathOnOpen(JS(e, "file", ""));
            if (path.empty()) continue;
            const bool isTex = (type == W3D_VIEW_TEXTURES && std::string(W3dCategoryByExtension(path)) == "textures");
            if (!isTex && (type < 0 || FileTypeFromPath(path) != type)) continue;   // (un tipo que no es de archivo)
            W3dExternalRefMark(path);
            const std::string k = KeyTexture(path), ch = W3dFolderNormalize(JS(e, "folder", ""));
            if (isTex) { gExternalTexBib.insert(k); if (!ch.empty()) gFolderTex[k] = ch; }
            else if (!ch.empty()) gFolderArch[k] = ch;
        }
}

void W3dResourcesViewCloseProject() {
    W3dResourceDeactivate();
    W3dThumbnailsForget();   // (las miniaturas de la cuadricula son del proyecto que se cierra)
    // un GRUPO de carpetas abierto (el modo mover de la biblioteca que quedo a medias) es del proyecto que se
    // cierra: su foto nombra carpetas y recursos de este. Se tira sin aplicar nada (si no, ninguna operacion de
    // carpetas volvia a tener undo y el proximo Esc del modo mover metia las carpetas de este en el otro)
    delete gGroupTx; gGroupTx = NULL;
    gGroupProf = 0;
    // (los bytes que retenian los pasos de undo son de este .w3d: no pueden reponerse en el proximo)
    for (std::set<RetainedEntry*>::iterator it = gRetained.begin(); it != gRetained.end(); ++it)
        std::vector<unsigned char>().swap((*it)->bytes);
    gCreated.clear();
    gOrderBib.clear(); OrderChange();
    gFolderTex.clear();
    gFolderArch.clear();
    gPurgedTex.clear();
    gPurgedArch.clear();
    gTexPrev.clear();
    gExternalTexBib.clear();
    W3dResourcesViewInvalidate();
}

void W3dResourcesViewInvalidate() {
    gInfoArch = InfoFile();   // (lo leido del archivo del ultimo recurso mirado)
    gUnreadableMats.clear();
    gScriptsRead = false;
    gScriptsText.clear();
    gScriptsName.clear();
    gEntriesRead = false;
    gEntriesTex.clear();
    gEntriesArchRead = false;
    gEntriesArch.clear();
}
