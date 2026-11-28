// ============================================================================
//  ProjectChanges.cpp — lo no guardado (ver el .h)
// ============================================================================
#include "io/ProjectChanges.h"
#include "animation/Animation.h"   // w3dGetTicks
#include "io/ProjectResources.h"
#include "io/ExternalLibrary.h"
#include "io/W3dContainer.h"
#include "io/EditedTexture.h"
#include "io/SaveAnimSets.h"
#include "importers/import_obj.h"   // PendingTextureOf: la textura base que todavia espera en la cola
#include "W3dRoots.h"
#include "io/EditorRoots.h"
#include "objects/MeshResource.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "animation/W3dAnimSet.h"
#include "animation/SkeletalAnimation.h"
#include "io/W3dResources.h"
#include "ViewPorts/IDE.h"
#include "ViewPorts/LayoutTree.h"
#include "config/W3dLang.h"
#include "WhiskUI/draw/icons.h"
#include "w3dGraphics.h"
#include "w3dTexture.h"   // Textures16Always
#include "w3dlog.h"
#include <map>
#include <set>
#include <cstdio>

void (*W3dChangesExitHook)() = 0;
void (*W3dChangesOpenHook)(const std::string&) = 0;
void (*W3dChangesNewHook)() = 0;
bool (*W3dChangesSaveHook)() = 0;
bool g_w3dChangesNoDialog = false;

// ============================================================================
//  LAS FIRMAS
// ============================================================================
static void Num(std::string& s, double v) { char b[40]; snprintf(b, sizeof(b), "%.6g;", v); s += b; }
static void Col(std::string& s, const float* c) { for (int k = 0; k < 4; k++) Num(s, c[k]); }
static std::string Path(const Texture* t) { return t ? W3dTextureKey(t->path) : std::string(); }
// la textura BASE de un material: la cargada o, si todavia no esta, la que espera en la cola diferida (o
// dormida). Al abrir, las texturas llegan de a una por cuadro DESPUES de la foto: con solo el puntero un
// material cambiaba de firma sin que nadie lo tocara (y todo proyecto con dos texturas quedaba "sin guardar")
static std::string BasePath(const Material* m) {
    if (m->texture) return Path(m->texture);
    const std::string p = PendingTextureOf(m);
    return p.empty() ? std::string() : W3dTextureKey(p);
}
// el contenido de un material (lo que se guarda de el; la carpeta es de la biblioteca)
static std::string SignatureMaterialFields(const Material* m) {
    std::string s = m->name + "\x01";
    Col(s, m->diffuse); Col(s, m->specular); Col(s, m->emission); Col(s, m->ambient);
    Num(s, m->shininess); Num(s, m->interpolation); Num(s, m->reflectMode); Num(s, m->rtRoughness); Num(s, m->rtMetallic);
    Num(s, m->depth_bias); Num(s, m->bias_meters); Num(s, m->order_pass); Num(s, m->mix); Num(s, m->thicknessLine);
    const bool bs[15] = { m->textureOn, m->filtered, m->transparent, m->vertexColor, m->lighting, m->repeat, m->uv8bit,
                          m->culling, m->depth_test, m->background, m->depth_write, m->chrome, m->normalMap, m->lines, m->perPixel };
    for (int k = 0; k < 15; k++) s += bs[k] ? '1' : '0';
    s += BasePath(m) + "\x01" + Path(m->normalTexture) + "\x01" + Path(m->textureB); Num(s, m->mixB); Num(s, m->toon); Num(s, m->outline);
    for (int k = 0; k < W3D_MAPS; k++) { s += "\x01" + Path(m->mapTex[k]); Num(s, (float)m->uvMap[k]); Num(s, m->mapWithTexture[k] ? 1.0f : 0.0f); }
    Num(s, m->ior); Num(s, m->transmission); s += std::string("\x01") + m->shaderFile + (m->shaderOn ? "1" : "0");
    for (size_t c = 0; c < m->layers.size(); c++) { s += "\x02" + Path(m->layers[c].tex); Num(s, m->layers[c].blend); Num(s, m->layers[c].uvMap); s += m->layers[c].on ? '1' : '0'; }
    // la fuerza de la emision (formato/material-animado.md)
    Num(s, m->emissionStrength);
    return s;
}
// ...firmado en su REPOSO si esta animado (lo que se guarda, ver W3dMaterialsRestBegin): mover el cabezal le cambia los
// numeros a un material animado, y eso no es un cambio sin guardar
static std::string SignatureMaterial(const Material* m) {
    std::vector<W3dMatRest> rest;
    W3dMaterialsRestBegin(rest, m);
    const std::string s = SignatureMaterialFields(m);
    W3dMaterialsRestEnd(rest);
    return s;
}
// el contenido de un animset CARGADO (sus clips y la version de cada clip de jerarquia)
static std::string SignatureAnimSet(const std::string& name) {
    std::vector<W3dResource*> all;
    W3dResourcesList(W3DREC_ANIMSET, all);
    for (size_t i = 0; i < all.size(); i++) {
        if (!all[i] || all[i]->state != W3DREC_READY || !all[i]->data) continue;
        const W3dAnimSet* set = (const W3dAnimSet*)all[i]->data;
        if (set->name != name) continue;
        std::string s;
        for (size_t k = 0; k < set->data.clips.size(); k++) {
            const SkeletalAnimation* a = set->data.clips[k];
            if (!a) continue;
            s += a->name; Num(s, a->startFrame); Num(s, a->endFrame); Num(s, a->FrameRate); Num(s, (double)a->tracks.size());
        }
        for (size_t k = 0; k < set->data.hierarchies.size(); k++) {
            const W3dClipHier* c = set->data.hierarchies[k];
            if (!c) continue;
            s += c->name; Num(s, c->version);
        }
        return s;
    }
    return std::string("-");   // (sin cargar: no se pudo tocar)
}
// la configuracion del proyecto (lo que va en project.json fuera de los recursos y las raices)
static std::string SignatureProject() {
    extern int AnimFPS;
    std::string s;
    Num(s, AnimFPS);
    s += w3dEngine::MipmapsGlobal() ? 'm' : '-';
    s += w3dEngine::Textures16Always() ? 't' : '-';
    s += w3dEngine::PixelatedGlobal() ? 'p' : '-';
    s += W3dInitialRoot() + "\x01";
    s += W3dExtLibsSignature();
    return s;
}

// ============================================================================
//  LA FOTO
// ============================================================================
static std::map<int, std::string> gMeshes;                 // serial del recurso -> nombre + version
static std::map<const Material*, std::string> gMats;      // material -> su firma
static std::set<std::string> gKeys;                       // TODAS las claves de biblioteca que habia
static std::map<std::string, std::string> gNames;         // clave -> nombre (la lista de BORRADOS)
static std::map<std::string, int> gIcons;                 // clave -> icono
static std::map<std::string, std::string> gAnims;         // animset -> su firma
static std::map<const Object*, std::string> gRoots;       // raiz -> nombre + tipo
static std::set<std::string> gRootsNames;                 // las raices sin cargar (por nombre)
static std::set<const Object*> gTouched;                  // las raices EDITADAS desde la foto
static std::string gFolders, gProject;
static bool gHasSnapshot = false;
static unsigned gVersion = 1;

static std::string SignatureMesh(const MeshResource* r) {
    std::string s = r->name + "\x01";
    Num(s, r->version);
    s += r->modified ? 'M' : '-';
    s += r->format < 0 ? '-' : r->format ? 'T' : 'B';   // el formato elegido en su tarjeta, sin guardar
    return s;
}

void W3dChangesSnapshot() {
    gMeshes.clear(); gMats.clear(); gKeys.clear(); gNames.clear(); gIcons.clear(); gAnims.clear();
    gRoots.clear(); gRootsNames.clear(); gTouched.clear();
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++) if (!reg[i]->deleted) gMeshes[reg[i]->serial] = SignatureMesh(reg[i]);
    for (size_t i = (size_t)MaterialsBase(); i < Materials.size(); i++)
        if (Materials[i] && Materials[i] != MaterialDefault) gMats[Materials[i]] = SignatureMaterial(Materials[i]);
    std::vector<W3dResourceItem> its;
    W3dLibraryList(-1, its);
    for (size_t i = 0; i < its.size(); i++) {
        const std::string k = W3dBibKey(its[i].type, its[i].id);
        gKeys.insert(k);
        gNames[k] = its[i].name;
        gIcons[k] = its[i].icon;
        if (its[i].type == W3D_VIEW_ANIMATIONS && its[i].parent.empty()) gAnims[its[i].id] = SignatureAnimSet(its[i].id);
    }
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++) {
        const std::string f = fs[i].name + "\x01" + W3dRootTypeKey(W3dRootTypeOf((int)i));
        if (fs[i].root) gRoots[fs[i].root] = f;
        else gRootsNames.insert(fs[i].name);
    }
    gFolders = W3dFoldersSignature(its);
    gProject = SignatureProject();
    gHasSnapshot = true;
    gVersion++;
}

static unsigned gEdits = 0;
unsigned W3dChangesEdits() { return gEdits; }
void W3dTouchedChanges() {
    gEdits++;
    const int a = W3dActiveRoot();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    if (a >= 0 && a < (int)fs.size() && fs[(size_t)a].root) {
        if (gTouched.insert(fs[(size_t)a].root).second) gVersion++;
    }
}
unsigned W3dChangesVersion() { return gVersion; }

// ============================================================================
//  LAS PREGUNTAS
// ============================================================================
// el IDE tiene ese script abierto con cambios sin guardar?
static bool DirtyScriptInIDE(const std::string& path) {
    struct J { static bool Find(ViewportBase* n, const std::string& r) {
        if (!n) return false;
        if (n->isLeaf()) return n->ViewportKind() == 8 && ((IDE*)n)->dirty && ((IDE*)n)->file == r;
        if (n->ContainerKind() == 1) return Find(((ViewportRow*)n)->childA, r) || Find(((ViewportRow*)n)->childB, r);
        return Find(((ViewportColumn*)n)->childA, r) || Find(((ViewportColumn*)n)->childB, r); } };
    return J::Find(LayoutCompleteRoot(), path);
}

bool W3dChangesResourceDirty(int type, const std::string& id) {
    if (!gHasSnapshot) return false;
    const std::string k = W3dBibKey(type, id);
    switch (type) {
        case W3D_VIEW_MESHES: {
            MeshResource* r = W3dMeshResourceByName(id);
            if (!r) return false;
            std::map<int, std::string>::iterator f = gMeshes.find(r->serial);
            return f == gMeshes.end() || f->second != SignatureMesh(r);
        }
        case W3D_VIEW_MATERIALS: {
            Material* m = W3dMaterialById(id);
            if (!m) return false;
            std::map<const Material*, std::string>::iterator f = gMats.find(m);
            return f == gMats.end() || f->second != SignatureMaterial(m);
        }
        case W3D_VIEW_TEXTURES: {
            if (!gKeys.count(k)) return true;
            if (W3dContainerEntryEdited(id)) return true;
            Texture* t = W3dLoadedTextureOf(id);
            return t && TexEditUnsaved(t);
        }
        case W3D_VIEW_SOUNDS: case W3D_VIEW_SCRIPTS: case W3D_VIEW_FONTS: case W3D_VIEW_VIDEOS: case W3D_VIEW_SHADERS:
            if (!gKeys.count(k)) return true;
            if (W3dContainerEntryEdited(id)) return true;
            return (type == W3D_VIEW_SCRIPTS || type == W3D_VIEW_SHADERS) && DirtyScriptInIDE(id);
        case W3D_VIEW_ANIMATIONS: {
            if (!gKeys.count(k)) return true;
            const size_t bar = id.find('/');
            const std::string set = (bar == std::string::npos) ? id : id.substr(0, bar);
            std::map<std::string, std::string>::iterator f = gAnims.find(set);
            return f == gAnims.end() || (f->second != "-" && f->second != SignatureAnimSet(set));
        }
        case W3D_VIEW_SCENES: case W3D_VIEW_PREFABS: {
            const int i = W3dRootFind(type == W3D_VIEW_PREFABS ? W3D_ROOT_PREFAB : W3D_ROOT_SCENE, id);
            return i >= 0 && W3dChangesRootDirty(i);
        }
    }
    return !gKeys.count(k);
}

bool W3dChangesRootDirty(int idx) {
    if (!gHasSnapshot) return false;
    const std::vector<W3dRootRow>& fs = W3dRoots();
    if (idx < 0 || idx >= (int)fs.size()) return false;
    const W3dRootRow& f = fs[(size_t)idx];
    if (!f.root) return !gRootsNames.count(f.name);   // (sin cargar: solo se pudo renombrar)
    if (gTouched.count(f.root)) return true;
    std::map<const Object*, std::string>::iterator it = gRoots.find(f.root);
    if (it == gRoots.end()) return !gRootsNames.count(f.name);   // nueva (una cargada despues de la foto tiene su nombre)
    return it->second != f.name + "\x01" + W3dRootTypeKey(W3dRootTypeOf(idx));
}

static std::string Type(int t) { return T(W3dViewSingular(t)); }

void W3dChangesList(std::vector<W3dChange>& out) {
    out.clear();
    if (!gHasSnapshot) return;
    if (SignatureProject() != gProject) {
        W3dChange c; c.icon = (int)IconType::save; c.type = T("Project"); c.name = T("Settings");
        out.push_back(c);
    }
    // las raices
    const std::vector<W3dRootRow>& fs = W3dRoots();
    std::set<std::string> rootsNow;
    for (size_t i = 0; i < fs.size(); i++) {
        rootsNow.insert(fs[i].name);
        if (!W3dChangesRootDirty((int)i)) continue;
        const int t = W3dRootTypeOf((int)i);
        W3dChange c; c.icon = W3dRootIcon(t);
        c.type = T(t == W3D_ROOT_GAME ? "Game" : t == W3D_ROOT_PREFAB ? "Prefab" : "Scene");
        c.name = fs[i].name;
        out.push_back(c);
    }
    // los recursos (las raices ya fueron: escenas y prefabs no se repiten)
    std::vector<W3dResourceItem> its;
    W3dLibraryList(-1, its);
    std::set<std::string> now;
    for (size_t i = 0; i < its.size(); i++) {
        const std::string k = W3dBibKey(its[i].type, its[i].id);
        now.insert(k);
        if (its[i].type == W3D_VIEW_SCENES || its[i].type == W3D_VIEW_PREFABS) continue;
        if (!W3dChangesResourceDirty(its[i].type, its[i].id)) continue;
        W3dChange c; c.icon = its[i].icon; c.type = Type(its[i].type); c.name = its[i].name;
        if (!gKeys.count(k)) c.name += std::string(" (") + T("new") + ")";
        out.push_back(c);
    }
    // lo BORRADO (o renombrado: el nombre de antes ya no esta)
    for (std::set<std::string>::iterator it = gKeys.begin(); it != gKeys.end(); ++it) {
        if (now.count(*it)) continue;
        int t = 0; std::string id;
        if (!W3dBibByKey(*it, &t, &id)) continue;
        if (t == W3D_VIEW_SCENES || t == W3D_VIEW_PREFABS) {
            if (rootsNow.count(id)) continue;
        }
        // (un material o una malla renombrados siguen vivos: su fila nueva ya dice que cambiaron)
        if (t == W3D_VIEW_MATERIALS) {
            bool isLive = false;
            for (std::map<const Material*, std::string>::iterator m = gMats.begin(); m != gMats.end() && !isLive; ++m) {
                if (m->second.compare(0, id.size() + 1, id + "\x01") != 0) continue;
                // (el puntero de la foto puede ser de un material ya LIBERADO -un proyecto que se cerro sin sacar
                //  otra foto-: solo se mira si sigue en la lista de materiales)
                bool inList = false;
                for (size_t q = 0; q < Materials.size() && !inList; q++) inList = (Materials[q] == m->first);
                if (inList && W3dMaterialById(m->first->name) == m->first) isLive = true;
            }
            if (isLive) continue;
        }
        if (t == W3D_VIEW_MESHES) {
            bool live = false;
            for (std::map<int, std::string>::iterator m = gMeshes.begin(); m != gMeshes.end() && !live; ++m) {
                if (m->second.compare(0, id.size() + 1, id + "\x01") != 0) continue;
                const MeshResource* r = W3dMeshResourceBySerial(m->first);
                live = r && !r->deleted;
            }
            if (live) continue;
        }
        W3dChange c; c.icon = gIcons.count(*it) ? gIcons[*it] : W3dIconView(t);
        c.type = Type(t);
        c.name = (gNames.count(*it) ? gNames[*it] : id) + " (" + T("deleted or renamed") + ")";
        out.push_back(c);
    }
    if (W3dFoldersSignature(its) != gFolders) {   // (con la biblioteca ya listada: listarla es lo que cuesta)
        W3dChange c; c.icon = (int)IconType::folder; c.type = T("Library"); c.name = T("Folders");
        out.push_back(c);
    }
}

bool W3dChangesHas() {
    std::vector<W3dChange> v;
    W3dChangesList(v);
    return !v.empty();
}
bool W3dChangesHasCache() {
    static unsigned last = 0, version = 0, edits = 0;
    static bool has = false, ready = false;
    const unsigned now = w3dGetTicks();
    // (una foto nueva -guardar/abrir- o una edicion nueva se ven en el acto; lo demas, cada medio segundo)
    if (!ready || version != gVersion || edits != gEdits || now - last > 500) {
        has = W3dChangesHas();
        last = now; version = gVersion; edits = gEdits; ready = true;
    }
    return has;
}

// ============================================================================
//  EL CARTEL (la accion pendiente mientras esta abierto)
// ============================================================================
static bool gDialog = false;
static int gAction = W3D_CHANGES_EXIT;
static std::string gPathOpen;
static std::string gSceneClose;   // W3D_CHANGES_CLOSE_SCENE: cual
extern void ChangesPopupOpen(int action);   // ui/ViewPorts/PopUp/ChangesPopup.cpp
// SIN CARTEL en las consolas (3DS, PSP, PS Vita): ni compilado (el cartel al cerrar colgaba todo: el que cierra es el
// sistema -HOME, el boton PS- y no espera). Symbian SI lo tiene: la tecla ROJA ya no cierra sola (Whisk3DAppUi.cpp
// la intercepta y pregunta); el administrador de tareas cierra de una sin preguntar (EEikCmdExit)
#if defined(W3D_3DS) || defined(W3D_PSP) || defined(W3D_VITA)
#define W3D_CHANGES_NO_DIALOG 1
#endif
// En PC, la web y Android el cartel ESTA: salir, abrir otro proyecto, empezar uno nuevo ("Nuevo proyecto" de la tarjeta
// Archivo) o cerrar una escena con algo sin guardar preguntan primero. (Estuvo apagado unas horas el 2026-11-15 y el dueno
// lo pidio de vuelta: "Nuevo proyecto" con el cubo movido borraba todo de una.) false lo apaga en todos lados; no es
// g_w3dChangesNoDialog: ese es el "sin preguntas" del harness, que tambien saltea el permiso de la red.
bool g_w3dChangesDialogActive = true;
void (*W3dChangesCloseSceneHook)(const std::string& name) = 0;
void W3dChangesForgetRoot(const Object* root) { if (root && gTouched.erase(root)) gVersion++; }
bool W3dChangesAskCloseScene(const std::string& name) {
#ifdef W3D_CHANGES_NO_DIALOG
    (void)name;
    return false;
#endif
    if (g_w3dChangesNoDialog || !g_w3dChangesDialogActive) return false;
    if (!W3dChangesRootDirty(W3dRootFind(W3D_ROOT_SCENE, name))) return false;
    gDialog = true;
    gAction = W3D_CHANGES_CLOSE_SCENE;
    gSceneClose = name;
    ChangesPopupOpen(W3D_CHANGES_CLOSE_SCENE);
    return true;
}
extern void ChangesPopupOpen(int action);   // ui/ViewPorts/PopUp/ChangesPopup.cpp

bool W3dChangesDialogOpen() { return gDialog; }

bool W3dChangesAsk(int action, const std::string& path) {
#ifdef W3D_CHANGES_NO_DIALOG
    // (Symbian y las consolas: ver W3D_CHANGES_NO_DIALOG arriba)
    (void)action; (void)path;
    return false;
#endif
    if (g_w3dChangesNoDialog || !g_w3dChangesDialogActive) return false;
    if (!W3dChangesHas()) return false;
    gDialog = true;
    gAction = action;
    gPathOpen = path;
    ChangesPopupOpen(action);
    return true;
}

static void Continue() {
    if (gAction == W3D_CHANGES_EXIT) { if (W3dChangesExitHook) W3dChangesExitHook(); }
    else if (gAction == W3D_CHANGES_OPEN) {
        if (W3dChangesOpenHook) W3dChangesOpenHook(gPathOpen);
        else {
            // (sin gancho -Symbian-: la apertura diferida de siempre, la que drena el loop de cada sistema)
            extern std::string g_projOpenPending;
            g_projOpenPending = gPathOpen;
        }
    }
    else if (gAction == W3D_CHANGES_NEW) { if (W3dChangesNewHook) W3dChangesNewHook(); }
    else if (gAction == W3D_CHANGES_CLOSE_SCENE) { if (W3dChangesCloseSceneHook) W3dChangesCloseSceneHook(gSceneClose); }
}
// "Guardar y ..." sin gancho (Symbian): el guardado de siempre sobre el archivo abierto
static bool SaveDefault() {
    extern std::string w3dPath;
    extern bool SaveW3D(const std::string&);
    if (w3dPath.empty()) return false;
    return SaveW3D(w3dPath);
}
void W3dChangesRespond(int r) {
    gDialog = false;
    if (r == 2) { w3dLogf("[cambios] cancelado"); return; }
    if (r == 0) {
        // "Guardar y ...": si el guardado no sale, no se sigue (se pierde lo que no se guardo). Con el Play
        // andando el juego se para ANTES: se guarda la escena del usuario, no la partida (SaveW3D no guarda
        // jugando: lo destruido por los scripts esta descolgado y lo instanciado lo borra el Stop)
        { extern void SimStop(); SimStop(); }
        const bool ok = W3dChangesSaveHook ? W3dChangesSaveHook() : SaveDefault();
        if (!ok) { w3dLogfW("[cambios] no se guardo: no sigo"); return; }
    }
    Continue();
}
