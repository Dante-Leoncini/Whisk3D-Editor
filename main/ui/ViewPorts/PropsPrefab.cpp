// ============================================================================
//  PropsPrefab.cpp — la tarjeta "Prefab Instance" del panel de propiedades
//  (objects/InstancePrefab.h). Vive aparte de Properties.cpp: el panel solo la
//  ARMA en su lugar (PropsPrefabBuild) y le pide el bindeo por frame
//  (PropsPrefabUpdate).
//    - en una INSTANCIA (pestania 2): su prefab (desplegable con los del proyecto;
//      acepta un prefab arrastrado de la biblioteca), cuantos overrides tiene, "Edit
//      Prefab" (abre la raiz del prefab), "Unpack" y "Reset Overrides";
//    - en un objeto GENERADO por una instancia (pestania 1, arriba del transform): de
//      que instancia es y "Select Instance" (lo generado no se edita: se edita el
//      prefab, o se desempaqueta la instancia).
//    - en un PROXY W3D (objects/ProxyW3d.h, pestania 2; la tarjeta se llama "Proxy
//      W3D"): su LIBRERIA (desplegable con las vinculadas + "Add Library..." con el
//      explorador de archivos), su ELEMENTO (desplegable con los prefabs y las escenas
//      de esa libreria; acepta uno arrastrado de la libreria en el outliner), cuantos
//      overrides tiene y "Reset Overrides". No se edita ni se desempaqueta: es de solo
//      lectura (se edita abriendo la libreria).
//    - en las dos, el STREAMING (io/Streaming.h): COMO se carga (desplegable "Always" / "By
//      Distance"), a que DISTANCIA y contra que OBJETIVO (un objeto por nombre; vacio = la camara
//      activa), el estado de su carga en la etiqueta, y la VISTA PREVIA del streaming en el editor
//      (una opcion del PROYECTO: se guarda en project.json).
//  Todo con Ctrl+Z salvo abrir el prefab (cambiar de raiz limpia el historial) y la vista previa.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"
#include "io/Libraries.h"
#include "io/ExternalLibrary.h"   // "Add Library...": vincular otro .w3d (con undo)
#include "ViewPorts/PopUp/FileBrowser.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"
#include "io/EditorRoots.h"
#include "W3dRoots.h"
#include "undo/Undo.h"
#include "ViewPorts/Notifications.h"
#include "render/RenderOptions.h"   // g_redraw
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"
#include "W3dLang.h"                // T(): los textos salen en el idioma del sistema
#include "io/Streaming.h"           // como se carga la instancia (carga / distancia / objetivo / vista previa)
#include "io/ProjectChanges.h"      // la vista previa es del proyecto: lo deja con cambios
#include <cstdio>

extern PopupMenu* OpenMenu;
void PropsLayoutDirty();   // (Properties.cpp: re-medir el panel el proximo cuadro)

static InstancePrefab* PfActive() {
    return (ActiveObj && W3dIsInstanceType(ActiveObj->getType())) ? (InstancePrefab*)ActiveObj : NULL;
}
static ProxyW3d* PxActive() {
    return (ActiveObj && ActiveObj->getType() == ObjectType::proxy) ? (ProxyW3d*)ActiveObj : NULL;
}

// ---- el desplegable: los prefabs del proyecto ----
static PopupMenu* gMenuPf = NULL;
static std::vector<std::string> gMenuPfNames;   // id - 1 -> nombre (se rearma en cada apertura)
static void ActionPickedPf(int id) {
    InstancePrefab* ip = PfActive();
    if (!ip || id < 1 || id > (int)gMenuPfNames.size()) return;
    std::string reason;
    if (!W3dPrefabChange(ip, gMenuPfNames[(size_t)(id - 1)], &reason) && !reason.empty())
        Notify(std::string(T(reason.c_str())), true);
    PropsLayoutDirty();
    g_redraw = true;
}
// ---- PROXY: los desplegables de su LIBRERIA y de su ELEMENTO ----
static PopupMenu* gMenuPxLib = NULL;
static std::vector<std::string> gMenuPxElem;   // id - 1 -> clave del elemento (W3dLibsKey)
static void ProxyToFirstElement(const std::string& lib) {
    ProxyW3d* px = PxActive();
    if (!px) return;
    std::vector<std::string> ps;
    int type = W3D_LIB_PREFAB;
    W3dLibsElements(lib, W3D_LIB_PREFAB, ps);
    if (ps.empty()) { type = W3D_LIB_SCENE; W3dLibsElements(lib, W3D_LIB_SCENE, ps); }
    std::string reason;
    if (!W3dProxyChange(px, lib, type, ps.empty() ? std::string() : ps[0], &reason) && !reason.empty())
        Notify(std::string(T(reason.c_str())), true);
    PropsLayoutDirty();
    g_redraw = true;
}
// "Add Library...": el .w3d elegido se VINCULA (con undo) y el proxy pasa a generar su primer elemento
static void PxPickedLib(const std::string& path) {
    std::string reason;
    if (!W3dExtLibLink(path, &reason)) {
        // (una ya vinculada: se usa esa)
        for (int i = 0; i < W3dLibsCount(); i++)
            if (W3dLibsDiskPath(i) == path) { ProxyToFirstElement(W3dLibsRow(i).name); return; }
        Notify(std::string(T("Not linked")) + ": " + T(reason.c_str()), true);
        return;
    }
    if (W3dLibsCount() > 0) ProxyToFirstElement(W3dLibsRow(W3dLibsCount() - 1).name);
}
static void ActionPickedPxLib(int id) {
    ProxyW3d* px = PxActive();
    if (!px) return;
    if (id == 1000) { OpenFileBrowser(T("Link .w3d..."), T("Link"), ".w3d", PxPickedLib, false); return; }
    if (id < 1 || id > W3dLibsCount()) return;
    const std::string lib = W3dLibsRow(id - 1).name;
    if (lib != px->Lib()) ProxyToFirstElement(lib);
}
static void ActionPxLibMenu() {
    ProxyW3d* px = PxActive();
    if (!px || !PropsActive || !PropsActive->propPxLib) return;
    if (!gMenuPxLib) { gMenuPxLib = new PopupMenu(); gMenuPxLib->action = ActionPickedPxLib; }
    gMenuPxLib->Clear();
    for (int i = 0; i < W3dLibsCount(); i++) {
        const bool current = (W3dLibsRow(i).name == px->Lib());
        MenuItem* it = gMenuPxLib->Add(W3dLibsRow(i).name, i + 1, current ? (int)IconType::notifOk : (int)IconType::lib);
        if (it) it->green = current;
    }
    gMenuPxLib->Add(T("Add Library..."), 1000, (int)IconType::lib);
    Button* b = PropsActive->propPxLib->button;
    gMenuPxLib->Resize();
    gMenuPxLib->Open(b->sx + b->width - gMenuPxLib->width, b->sy + b->height - GlobalScale, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuPxLib;
}
// "Explore...": la libreria POR DENTRO (sus escenas como carpetas, sus objetos y sus prefabs): lo elegido pasa a ser el
// elemento del proxy (un objeto de una escena, con todo lo que cuelga de el)
static void PxBrowsePicked(const std::string& picked) {
    ProxyW3d* px = PxActive();
    if (!px) return;
    extern bool LayoutPickedProxy(const std::string&, const std::string&, int*, std::string*);
    int type = W3D_LIB_PREFAB; std::string elem;
    if (!LayoutPickedProxy(px->Lib(), picked, &type, &elem)) return;
    std::string reason;
    if (!W3dProxyChange(px, px->Lib(), type, elem, &reason) && !reason.empty()) Notify(std::string(T(reason.c_str())), true);
    PropsLayoutDirty();
    g_redraw = true;
}
// el desplegable del ELEMENTO de un proxy (usa el de la instancia: propPfSel)
static void ActionPickedPxElem(int id) {
    ProxyW3d* px = PxActive();
    if (id == 9000) { if (px && !px->Lib().empty()) OpenFileBrowserLib(T("Proxy W3D"), T("Use"), px->Lib(), PxBrowsePicked); return; }
    if (!px || id < 1 || id > (int)gMenuPxElem.size()) return;
    std::string lib, elem; int type = W3D_LIB_PREFAB;
    if (!W3dLibsByKey(gMenuPxElem[(size_t)(id - 1)], &lib, &type, &elem)) return;
    std::string reason;
    if (!W3dProxyChange(px, lib, type, elem, &reason) && !reason.empty()) Notify(std::string(T(reason.c_str())), true);
    PropsLayoutDirty();
    g_redraw = true;
}
static PopupMenu* gMenuPxElemPop = NULL;
static void ActionPxElemMenu(ProxyW3d* px) {
    if (!gMenuPxElemPop) { gMenuPxElemPop = new PopupMenu(); gMenuPxElemPop->action = ActionPickedPxElem; }
    gMenuPxElemPop->Clear();
    gMenuPxElem.clear();
    const std::string lib = px->Lib();
    for (int t = 0; t < 2 && !lib.empty(); t++) {
        const int type = (t == 0) ? W3D_LIB_PREFAB : W3D_LIB_SCENE;
        std::vector<std::string> is;
        W3dLibsElements(lib, type, is);
        for (size_t i = 0; i < is.size(); i++) {
            const std::string key = W3dLibsKey(lib, type, is[i]);
            gMenuPxElem.push_back(key);
            const bool current = (key == px->prefab);
            MenuItem* it = gMenuPxElemPop->Add(is[i], (int)gMenuPxElem.size(),
                                                   current ? (int)IconType::notifOk : (type == W3D_LIB_PREFAB ? (int)IconType::prefab : (int)IconType::camera));
            if (it) it->green = current;
        }
    }
    if (gMenuPxElem.empty()) gMenuPxElemPop->Add(T("No prefabs yet"), 0, -1);
    // (un OBJETO de una escena, o ver todo lo que tiene: el .w3d por dentro)
    if (!lib.empty()) { MenuItem* ex = gMenuPxElemPop->Add(T("Explore..."), 9000, (int)IconType::folder); if (ex) ex->lineUp = true; }
    Button* b = PropsActive->propPfSel->button;
    gMenuPxElemPop->Resize();
    gMenuPxElemPop->Open(b->sx + b->width - gMenuPxElemPop->width, b->sy + b->height - GlobalScale, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuPxElemPop;
}

static void ActionPfMenu() {
    InstancePrefab* ip = PfActive();
    if (!ip || !PropsActive || !PropsActive->propPfSel) return;
    if (PxActive()) { ActionPxElemMenu(PxActive()); return; }
    if (!gMenuPf) { gMenuPf = new PopupMenu(); gMenuPf->action = ActionPickedPf; }
    gMenuPf->Clear();
    gMenuPfNames.clear();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++) {
        if (fs[i].type != W3D_ROOT_PREFAB) continue;
        gMenuPfNames.push_back(fs[i].name);
        const bool current = (fs[i].name == ip->prefab);
        MenuItem* it = gMenuPf->Add(fs[i].name, (int)gMenuPfNames.size(), current ? (int)IconType::notifOk : (int)IconType::prefab);
        if (it) it->green = current;
    }
    // las ESCENAS del proyecto (una escena tambien se instancia: "escena:<nombre>"), menos la que se edita
    for (size_t i = 0; i < fs.size(); i++) {
        if (!W3dRootEs3D(fs[i].type) || (int)i == W3dActiveRoot()) continue;
        const std::string key = W3dPrefabKeyScene(fs[i].name);
        gMenuPfNames.push_back(key);
        const bool current = (key == ip->prefab);
        MenuItem* it = gMenuPf->Add(fs[i].name, (int)gMenuPfNames.size(), current ? (int)IconType::notifOk : W3dRootIcon(fs[i].type));
        if (it) it->green = current;
    }
    Button* b = PropsActive->propPfSel->button;
    gMenuPf->Resize();
    gMenuPf->Open(b->sx + b->width - gMenuPf->width, b->sy + b->height - GlobalScale, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuPf;
}
static void ActionPfEdit() {
    InstancePrefab* ip = PfActive();
    if (!ip) return;
    std::string esc;
    const bool isScene = W3dPrefabIsScene(ip->prefab, &esc);   // (la de una ESCENA: se edita la escena)
    const int idx = isScene ? W3dRootFind(W3D_ROOT_SCENE, esc) : W3dRootFind(W3D_ROOT_PREFAB, ip->prefab);
    std::string reason;
    if (idx < 0) { Notify(std::string(T(isScene ? "There is no such scene" : "There is no such prefab")), true); return; }
    if (!W3dActivateRoot(idx, &reason)) Notify(std::string(T(reason.c_str())), true);
}
static void ActionPfUnpack() {
    InstancePrefab* ip = PfActive();
    std::string reason;
    if (ip && !W3dPrefabUnpack(ip, &reason) && !reason.empty()) Notify(std::string(T(reason.c_str())), true);
    PropsLayoutDirty();
    g_redraw = true;
}
// Reset Overrides con Ctrl+Z (io/PrefabsEditor.h: regenera sin liberar lo generado, el paso se queda con la
// generacion que sale; no con el Play andando)
static void ActionPfReset() {
    InstancePrefab* ip = PfActive();
    if (!ip) return;
    std::string reason;
    if (!W3dPrefabResetOverrides(ip, &reason) && !reason.empty()) Notify(std::string(T(reason.c_str())), true);
    PropsLayoutDirty();
    g_redraw = true;
}
static void ActionPfInstance() {
    Object* inst = ActiveObj ? W3dInstanceOf(ActiveObj) : NULL;
    // (la de AFUERA de todo: la que el usuario puso en la escena)
    while (inst && W3dInstanceOf(inst)) inst = W3dInstanceOf(inst);
    if (!inst) return;
    DeselectAll();
    inst->Select();
    PropsLayoutDirty();
    g_redraw = true;
}

// ---- el STREAMING de la instancia: COMO se carga ----
static PopupMenu* gMenuLoad = NULL;
static void ActionPickedLoad(int id) {
    InstancePrefab* ip = PfActive();
    if (!ip || id < 1 || id > 2) return;
    const int c = (id == 2) ? W3D_LOAD_DISTANCE : W3D_LOAD_ALWAYS;
    if (c != ip->load) W3dInstanceLoadChange(ip, c, ip->distance, ip->target);
    PropsLayoutDirty();
    g_redraw = true;
}
static void ActionLoadMenu() {
    InstancePrefab* ip = PfActive();
    if (!ip || !PropsActive || !PropsActive->propPfLoad) return;
    if (!gMenuLoad) { gMenuLoad = new PopupMenu(); gMenuLoad->action = ActionPickedLoad; }
    gMenuLoad->Clear();
    MenuItem* a = gMenuLoad->Add(T("Always"), 1, ip->load == W3D_LOAD_ALWAYS ? (int)IconType::notifOk : -1);
    if (a) a->green = (ip->load == W3D_LOAD_ALWAYS);
    MenuItem* d = gMenuLoad->Add(T("By Distance"), 2, ip->load == W3D_LOAD_DISTANCE ? (int)IconType::notifOk : -1);
    if (d) d->green = (ip->load == W3D_LOAD_DISTANCE);
    Button* b = PropsActive->propPfLoad->button;
    gMenuLoad->Resize();
    gMenuLoad->Open(b->sx + b->width - gMenuLoad->width, b->sy + b->height - GlobalScale, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuLoad;
}
// la VISTA PREVIA (opcion del proyecto): al apagarla, el editor vuelve a ver el nivel entero
static void ActionPreviewView() {
    W3dStreamingPreviewViewSet(g_w3dStreamingPreviewView);
    W3dTouchedChanges();   // (es del proyecto: el '*' del titulo; se guarda en project.json)
    g_redraw = true;
}

void PropsPrefabBuild(Properties* p) {
    if (!p) return;
    p->propPrefab = new GroupPropertie(T("Prefab Instance"));
    p->propPrefab->icon = (int)IconType::prefab;
    // (un PROXY: su libreria, arriba del elemento)
    p->propPxLib = new PropButton("", IconType::lib);
    p->propPxLib->button->dropdown = true;
    p->propPxLib->action = ActionPxLibMenu;
    p->propPxLib->hidden = true;
    p->propPrefab->properties.push_back(p->propPxLib);
    p->propPfSel = new PropButton("", IconType::prefab);
    p->propPfSel->button->dropdown = true;
    p->propPfSel->action = ActionPfMenu;
    p->propPrefab->properties.push_back(p->propPfSel);
    p->propPfInfo = new PropLabel("");
    p->propPrefab->properties.push_back(p->propPfInfo);
    // el STREAMING: como se carga, a que distancia, contra que objetivo; y la vista previa del proyecto
    p->propPfLoad = new PropButton(T("Loading mode"));
    p->propPfLoad->withLabel = true;   // label a la izquierda, el desplegable a la derecha
    p->propPfLoad->button->dropdown = true;
    p->propPfLoad->action = ActionLoadMenu;
    p->propPfLoad->hidden = true;
    p->propPrefab->properties.push_back(p->propPfLoad);
    p->propPfDistance = new PropFloat(T("Distance"), "m");
    p->propPfDistance->SetRange(0.0f, 1.0e7f);
    p->propPfDistance->fineStep = 1.0f; p->propPfDistance->thickStep = 10.0f;
    p->propPrefab->properties.push_back(p->propPfDistance);
    p->propPfTarget = new PropText(T("Target"), "");
    p->propPfTarget->hidden = true;
    p->propPrefab->properties.push_back(p->propPfTarget);
    p->propPfView = new PropBool(T("Streaming preview"));
    p->propPfView->onChange = ActionPreviewView;
    p->propPrefab->properties.push_back(p->propPfView);
    p->propPfEdit = new PropButton(T("Edit Prefab"), IconType::prefab);
    p->propPfEdit->action = ActionPfEdit;
    p->propPrefab->properties.push_back(p->propPfEdit);
    p->propPfUnpack = new PropButton(T("Unpack"), IconType::folder);
    p->propPfUnpack->action = ActionPfUnpack;
    p->propPrefab->properties.push_back(p->propPfUnpack);
    p->propPfReset = new PropButton(T("Reset Overrides"), IconType::trash);
    p->propPfReset->action = ActionPfReset;
    p->propPrefab->properties.push_back(p->propPfReset);
    p->propPfInstance = new PropButton(T("Select Instance"), IconType::prefab);
    p->propPfInstance->action = ActionPfInstance;
    p->propPrefab->properties.push_back(p->propPfInstance);
    p->GroupProperties.push_back(p->propPrefab);
}

// ---- el STREAMING de la tarjeta, con Ctrl+Z: los valores se editan EN EL LUGAR (el numero se arrastra, el objetivo
// se tipea) y el paso se anota cuando la edicion TERMINA (mouse suelto, sin tipear): uno por arrastre, no uno por
// pixel. 'gSeen' es como estaba la instancia la ultima vez que se anoto
namespace {
struct LoadView { InstancePrefab* ip; unsigned serial; int load; float distance; std::string target;
                    LoadView() : ip(NULL), serial(0), load(0), distance(0.0f) {} };
LoadView gSeen;
std::string gTypedTarget;
}
static void ShowLoad(InstancePrefab* ip) {
    gSeen.ip = ip; gSeen.serial = ip ? ip->serial : 0;
    if (ip) { gSeen.load = ip->load; gSeen.distance = ip->distance; gSeen.target = ip->target; }
}
static bool LoadDiffers(const InstancePrefab* ip) {
    return ip->load != gSeen.load || ip->distance != gSeen.distance || ip->target != gSeen.target;
}
static void SyncLoad(Properties* p, InstancePrefab* ip) {
    extern bool leftMouseDown;
    // un paso anotado por otro lado o un Ctrl+Z / Ctrl+Y de uno: lo de ahora es lo visto (no es una edicion nueva)
    static unsigned version = 0;
    if (version != g_w3dLoadVersion) {
        version = g_w3dLoadVersion;
        if (gSeen.ip == ip) ShowLoad(ip);
    }
    // la instancia que se miraba cambio (otro objeto activo; o se libero y otra nacio en su direccion): lo que quedo
    // editado en la anterior se anota igual
    if (gSeen.ip != ip || (ip && gSeen.serial != ip->serial)) {
        if (gSeen.ip && W3dLiveObjectSerial(gSeen.ip, gSeen.serial) && LoadDiffers(gSeen.ip))
            W3dInstanceLoadUndo(gSeen.ip, gSeen.load, gSeen.distance, gSeen.target);
        ShowLoad(ip);
        gTypedTarget.clear();
    }
    if (!ip) return;
    // el OBJETIVO: lo tipeado se escribe en vivo; sin foco muestra el valor real (un Ctrl+Z, otro objeto)
    const bool typing = p->propPfTarget && TextFieldLive(&p->propPfTarget->field);
    if (p->propPfTarget) {
        if (typing && p->propPfTarget->field.text != gTypedTarget) {
            ip->target = p->propPfTarget->field.text;
            gTypedTarget = ip->target;
        }
        if (!typing) {
            gTypedTarget.clear();
            if (p->propPfTarget->field.text != ip->target) { p->propPfTarget->field.SetText(ip->target); g_redraw = true; }
        }
    }
    const bool editing = leftMouseDown || typing || (g_propFloatEditing && g_propFloatEditing == p->propPfDistance);
    if (!editing && LoadDiffers(ip)) {
        W3dInstanceLoadUndo(ip, gSeen.load, gSeen.distance, gSeen.target);
        ShowLoad(ip);
    }
}

// por frame (compara antes de asignar: sin redibujado infinito)
void PropsPrefabUpdate(Properties* p, int tab) {
    if (!p || !p->propPrefab) return;
    InstancePrefab* ip = (tab == 2) ? PfActive() : NULL;
    Object* gen = (tab == 1 && ActiveObj && W3dIsGenerated(ActiveObj)) ? ActiveObj : NULL;
    const bool vis = (ip != NULL) || (gen != NULL);
    if (p->propPrefab->visible != vis) { p->propPrefab->visible = vis; PropsLayoutDirty(); }
    SyncLoad(p, ip);   // (tambien sin tarjeta: lo que quedo editado en la instancia anterior se anota)
    if (!vis) {
        if (p->propPfDistance) p->propPfDistance->value = NULL;
        if (p->propPfView) p->propPfView->value = NULL;
        return;
    }
    bool layout = false;
    const bool isInst = (ip != NULL);
    ProxyW3d* px = (ip && ip->getType() == ObjectType::proxy) ? (ProxyW3d*)ip : NULL;
    const bool isProxy = (px != NULL);
    // la tarjeta de un PROXY se llama distinto y no tiene Edit Prefab ni Unpack (es de solo lectura)
    // (la de una ESCENA: "Scene Instance" y "Edit Scene")
    const bool isScene = ip && !isProxy && W3dPrefabIsScene(ip->prefab);
    {
        const std::string title = isProxy ? std::string(T("Proxy W3D")) : isScene ? std::string(T("Scene Instance"))
                                                                                    : std::string(T("Prefab Instance"));
        const int icon = isProxy ? (int)IconType::lib : isScene ? (int)IconType::camera : (int)IconType::prefab;
        if (p->propPrefab->name != title || p->propPrefab->icon != icon) {
            p->propPrefab->name = title; p->propPrefab->icon = icon; layout = true;
        }
        const std::string ed = T(isScene ? "Edit Scene" : "Edit Prefab");
        if (p->propPfEdit->button->text != ed) {
            p->propPfEdit->button->text = ed;
            p->propPfEdit->button->icon = isScene ? (int)IconType::camera : (int)IconType::prefab;
            layout = true;
        }
    }
    if (p->propPxLib && p->propPxLib->hidden == isProxy) { p->propPxLib->hidden = !isProxy; layout = true; }
    if (p->propPfSel->hidden == isInst) { p->propPfSel->hidden = !isInst; layout = true; }
    if (p->propPfEdit->hidden == (isInst && !isProxy)) { p->propPfEdit->hidden = !(isInst && !isProxy); layout = true; }
    if (p->propPfUnpack->hidden == (isInst && !isProxy)) { p->propPfUnpack->hidden = !(isInst && !isProxy); layout = true; }
    if (p->propPfInstance->hidden != isInst) { p->propPfInstance->hidden = isInst; layout = true; }
    // el STREAMING (solo en una instancia o un proxy): la distancia y el objetivo solo si es por distancia
    {
        const bool dist = isInst && ip->load == W3D_LOAD_DISTANCE;
        if (p->propPfLoad && p->propPfLoad->hidden == isInst) { p->propPfLoad->hidden = !isInst; layout = true; }
        if (p->propPfTarget && p->propPfTarget->hidden == dist) { p->propPfTarget->hidden = !dist; layout = true; }
        float* dv = dist ? &ip->distance : NULL;
        if (p->propPfDistance && p->propPfDistance->value != dv) { p->propPfDistance->value = dv; layout = true; }
        bool* vv = isInst ? &g_w3dStreamingPreviewView : NULL;
        if (p->propPfView && p->propPfView->value != vv) { p->propPfView->value = vv; layout = true; }
        if (isInst && p->propPfLoad) {
            const std::string t = (ip->load == W3D_LOAD_DISTANCE) ? std::string(T("By Distance")) : std::string(T("Always"));
            if (p->propPfLoad->button->text != t) { p->propPfLoad->button->text = t; g_redraw = true; }
        }
    }
    std::string info;
    if (ip) {
        W3dPrefabSyncOverrides(ip);   // (lo que el usuario cambio en lo generado desde el ultimo cuadro)
        std::string sel = ip->prefab.empty() ? std::string(T("(none)")) : ip->prefab;
        W3dPrefabIsScene(ip->prefab, &sel);   // (la de una escena muestra el nombre de la escena)
        if (px) {
            // el proxy: su libreria (con "(!)" si no esta vinculada) y su elemento
            std::string lib = px->Lib();
            if (lib.empty()) lib = T("(none)");
            else if (W3dLibsFind(lib) < 0) lib += " (!)";
            if (p->propPxLib->button->text != lib) { p->propPxLib->button->text = lib; g_redraw = true; }
            sel = px->Element().empty() ? std::string(T("(none)")) : px->Element();
            if (ip->notGenerated && !px->Element().empty()) sel += " (!)";
        } else if (ip->notGenerated && !ip->prefab.empty()) sel += " (!)";
        if (p->propPfSel->button->text != sel) { p->propPfSel->button->text = sel; g_redraw = true; }
        const int n = (int)(ip->overProps.size() + ip->overVisible.size());
        char b[160];
        // (una diferida dice ademas en que esta su carga)
        if (ip->load == W3D_LOAD_DISTANCE || ip->streamState != W3D_STREAM_LOADED)
            snprintf(b, sizeof(b), "%s: %d. %s", T("Overrides"), n,
                     ip->streamState == W3D_STREAM_LOADED ? T("Loaded") :
                     ip->streamState == W3D_STREAM_REQUESTING ? T("Loading...") : T("Unloaded"));
        else snprintf(b, sizeof(b), "%s: %d", T("Overrides"), n);
        info = b;
        const bool noOv = (n == 0);
        if (p->propPfReset->hidden != noOv) { p->propPfReset->hidden = noOv; layout = true; }
    } else {
        Object* inst = W3dInstanceOf(gen);
        // (lo generado no se edita: lo unico propio que se guarda es su visible y los valores de sus scripts)
        info = std::string(T("Generated by")) + " " + (inst ? inst->name : std::string("?")) + ". " +
               T("Only its visibility and script values are saved");
        if (!p->propPfReset->hidden) { p->propPfReset->hidden = true; layout = true; }
    }
    if (p->propPfInfo->name != info) { p->propPfInfo->name = info; g_redraw = true; }
    if (layout) PropsLayoutDirty();
}
