// ============================================================================
//  PropsRoutine.cpp — la tarjeta "Routine" del panel de propiedades (pestania 2
//  del objeto Rutina, objects/Routine.h). Patron de PropsHitbox: el panel la ARMA
//  (PropsRoutineBuild, en BuildGroups) y le pide el bindeo por frame
//  (PropsRoutineUpdate, en UpdateTabs).
//
//  - "Render mode": que lista se edita (la de todos, "All", o la de un modo).
//  - "Use": que lista usa ese modo (la de todos, la propia o la de otro modo).
//  - los PASOS: lista con icono por grupo, Add (desplegable AGRUPADO) / Remove /
//    Move Up / Move Down.
//  - los campos del paso elegido, cada uno lo que corresponde:
//      la MALLA se elige de un desplegable por CARPETAS (las de la biblioteca):
//      carpeta > malla > parte (una malla de una sola parte se elige directo);
//      las texturas por sus carpetas; los objetos y las rutinas de una lista;
//      la info de la malla en FILAS (triangulos / vertices / indices);
//      la PRIMITIVA (puntos, lineas, tira, abanico... y las simuladas, con aviso);
//      los COLORES con el selector de color de Whisk3D (o los 4 numeros, para
//      usar memorias "@nombre[i]");
//      la luz, la opcion, el on/off, el rango manual, el array, Max...
//  - el MOTIVO, en rojo, si la rutina no pasa la validacion del editor.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/Routine.h"
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "objects/MeshResource.h"
#include "objects/MeshFlows.h"
#include "io/ProjectResources.h"   // el paso "Shader": los .glsl de la biblioteca
#include "io/W3dContainer.h"       // W3dImportAsset: una textura cargada de afuera entra al proyecto
#include "ViewPorts/PopUp/FileBrowser.h"   // "Cargar textura": el explorador
#include "W3dLang.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "ViewPorts/W3dInput.h"   // W3dK_*: las teclas de la lista de pasos
#include <algorithm>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void PropsLayoutDirty();   // (Properties.cpp: re-armar el layout del panel)
void PropsOpenMenuUnderButton(PopupMenu* menu, Button* button);   // (Properties.cpp: el estandar de los desplegables)
bool W3dRoutineValidate(Routine* r);
// (edit/RoutineEditor.cpp) los iconos y el menu de pasos agrupado
int  W3dRoutineStepIcon(int type);
int  W3dRoutineIcon(Routine* r);
void W3dRoutineMenuSteps(PopupMenu* root, std::vector<PopupMenu*>& subs, void (*action)(int), bool withEmpty);

static Properties* gPr = NULL;

static Routine* RuActive() {
    return (ActiveObj && ActiveObj->getType() == ObjectType::routine) ? (Routine*)ActiveObj : NULL;
}
static std::vector<W3dStep>* RuList(Routine* r) {
    if (!r) return NULL;
    if (r->editedList < 0 || r->editedList >= Routine::ModeN) r->editedList = 0;
    return &r->lists[r->editedList];
}
static W3dStep* RuStep(Routine* r) {
    std::vector<W3dStep>* L = RuList(r);
    if (!L || r->activeStep < 0 || r->activeStep >= (int)L->size()) return NULL;
    return &(*L)[r->activeStep];
}
static const char* kModesUI[Routine::ModeN] = { "All", "Solid", "Material", "Rendered", "Wireframe", "Z-Buffer" };
static void Change(Routine* r) { r->dirty = true; g_w3dRoutinesGen++; g_redraw = true; }

static std::string Base(const std::string& path) {
    const size_t b = path.find_last_of("/\\");
    return b == std::string::npos ? path : path.substr(b + 1);
}
static std::string NumText(const W3dNum& n) {
    if (!n.ref.empty()) return n.ref;
    char b[32]; snprintf(b, sizeof b, "%g", n.v); return b;
}
// la malla de un paso ya resuelta (o NULL)
static MeshResource* MeshOf(const W3dStep* p) {
    if (!p || W3dStepRef(p->type) != RefMesh || !p->ptr) return NULL;
    MeshResource* m = (MeshResource*)p->ptr;
    return W3dMeshResourceLive(m) ? m : NULL;
}
static bool UsesParts(int t) { return t == StepDraw || t == StepVisible; }

// ---- la lista de pasos (modo 14 de PropListMeshParts: va por 'obj') ----
static int RuRows(Object* o) {
    if (!o || o->getType() != ObjectType::routine) return 0;
    std::vector<W3dStep>* L = RuList((Routine*)o);
    return L ? (int)L->size() : 0;
}
static const W3dStep* RuRow(Object* o, int i) {
    if (!o || o->getType() != ObjectType::routine) return NULL;
    std::vector<W3dStep>* L = RuList((Routine*)o);
    return (L && i >= 0 && i < (int)L->size()) ? &(*L)[i] : NULL;
}
// un color de numeros fijos (con alguna memoria se muestran los numeros: el color cambia jugando)
// un "Clear color" en AUTO no tiene color propio (toma el fondo): ni cuadro ni numeros
static bool ColorAuto(const W3dStep& p) { return p.type == StepColorClear && p.mode == ColorClearAuto; }
static bool ColorFixed(const W3dStep& p) {
    if (!W3dStepIsColor(p.type) || ColorAuto(p)) return false;
    for (int k = 0; k < 4; k++) if (!p.n[k].ref.empty()) return false;
    return true;
}
static std::string RuRowText(Object* o, int i) {
    const W3dStep* pp = RuRow(o, i);
    if (!pp) return std::string();
    const W3dStep& p = *pp;
    // (sin numero ni "on/off" ni los numeros de un color: el icono va a la izquierda y el checkbox o el color a la
    //  derecha, RuRowValue)
    char b[64];
    if (p.type == StepArray)   // "Array: Normals"
        return std::string(T("Array")) + ": " + T(W3dOptionLabel(OptArray, p.mode));
    std::string t = T(W3dStepLabel(p.type));
    if (W3dStepUsesOn(p.type)) {   // "Textura on/off" -> "Textura": lo dice el checkbox
        const size_t k = t.rfind(" on/off");
        if (k != std::string::npos && k + 7 == t.size()) t.erase(k);
    }
    if (p.type == StepClear) {   // "Clear color + depth" (y con el color: que hace, si no es Auto)
        static const char* const kBuf[3] = { "color", "depth", "stencil" };
        std::string q;
        for (int k = 0; k < 3; k++) if (p.integer & (1 << k)) q += std::string(q.empty() ? " " : " + ") + T(kBuf[k]);
        if (q.empty()) return t + " (" + T("nothing") + ")";
        if ((p.integer & ClearColor) && p.mode == ClearBackgroundColor) q += std::string(" (") + T("Clear with color") + ")";
        if ((p.integer & ClearColor) && p.mode == ClearBackgroundOwn)
            q += " (" + (p.ref.empty() ? std::string(T("Draw custom background")) : Base(p.ref)) + ")";
        return t + q;
    }
    if (p.type == StepColorClear && ColorAuto(p)) return t + " (" + T("Auto") + ")";
    if (W3dStepInteger(p.type) == IntegerLight) { snprintf(b, sizeof b, " %d", p.integer); t += b; }
    if (!p.ref.empty()) t += " " + ((W3dStepRef(p.type) == RefTexture || W3dStepRef(p.type) == RefShader) ? Base(p.ref) : p.ref);
    if (UsesParts(p.type) && p.integer >= 0) { snprintf(b, sizeof b, " [%d]", p.integer); t += b; }
    if (W3dStepOptions(p.type) != OptNothing) t += std::string(" ") + T(W3dOptionLabel(W3dStepOptions(p.type), p.mode));
    if (p.type == StepDraw) {
        if (p.sub == RangeManual)
            t += " " + NumText(p.n[0]) + ".." + ((p.n[1].ref.empty() && p.n[1].v < 0) ? std::string("max") : NumText(p.n[1]));
        else if (p.sub == RangeArray) t += " " + p.n[2].ref + (p.n[3].ref.empty() ? std::string() : " / " + p.n[3].ref);
        return t;
    }
    if (p.type == StepVisible) return t + " -> " + p.n[0].ref + (p.ref2.empty() ? std::string() : " (" + p.ref2 + ")");
    if (p.type == StepLua) return t + (p.ref2.empty() ? std::string() : " (" + p.ref2 + ")");
    if (p.type == StepSkipIf || p.type == StepSkipHidden || p.type == StepSkipIfNone || p.type == StepSkipOutside || p.type == StepSkipRail || p.type == StepSkipNoListing) { snprintf(b, sizeof b, T("skip %d"), p.integer); t += " "; t += b; }
    const int nn = W3dStepNumbers(p);
    if (nn > 0 && !ColorFixed(p)) {
        t += " (";
        for (int k = 0; k < nn; k++) { if (k) t += ", "; t += NumText(p.n[k]); }
        t += ")";
    }
    return t;
}
// lo que se ve a la DERECHA de la fila (PropList): el checkbox de un on/off, el cuadro de un color fijo
static int RuRowValue(Object* o, int i, float* rgba) {
    const W3dStep* p = RuRow(o, i);
    if (!p) return 0;
    if (W3dStepUsesOn(p->type)) return p->on ? 2 : 1;
    if (ColorFixed(*p)) {
        for (int k = 0; k < 4; k++) rgba[k] = p->n[k].v < 0 ? 0.0f : (p->n[k].v > 1 ? 1.0f : p->n[k].v);
        return 3;
    }
    return 0;
}
static int RuRowIcon(Object* o, int i) {
    const W3dStep* p = RuRow(o, i);
    if (p && p->type == StepRoutine && p->ptr) return W3dRoutineIcon((Routine*)p->ptr);   // (una de material: su icono)
    return p ? W3dRoutineStepIcon(p->type) : -1;
}
static void RuRowSelect(Object* o, int i) {
    if (!o || o->getType() != ObjectType::routine) return;
    ((Routine*)o)->activeStep = i;
    PropsLayoutDirty();   // cambian los campos visibles
}

// ---- "Render mode" y "Use" ----
static PopupMenu* MenuRuList = NULL;
static PopupMenu* MenuRuUse = NULL;
static void ActionPickedList(int id) {
    Routine* r = RuActive(); if (!r) return;
    r->editedList = id; r->activeStep = RuList(r)->empty() ? -1 : 0;
    PropsLayoutDirty(); g_redraw = true;
}
static void ActionMenuList() {
    Routine* r = RuActive();
    if (!gPr || !gPr->propRuList || !r) return;
    if (!MenuRuList) { MenuRuList = new PopupMenu(); MenuRuList->action = ActionPickedList; }
    MenuRuList->Clear();
    for (int m = 0; m < Routine::ModeN; m++)
        MenuRuList->Add(T(kModesUI[m]), m, (int)IconType::camera)->green = (m == r->editedList);
    PropsOpenMenuUnderButton(MenuRuList, gPr->propRuList->button);
}
// id: 0 = la de todos; 1 = la propia; 2.. = la de otro modo (2 + modo)
static void ActionUsePicked(int id) {
    Routine* r = RuActive(); if (!r || r->editedList <= 0) return;
    const int m = r->editedList;
    if (id == 0) r->use[m] = -1;
    else if (id == 1) r->use[m] = (signed char)m;
    else r->use[m] = (signed char)(id - 2);
    Change(r);
}
static void ActionMenuUse() {
    Routine* r = RuActive();
    if (!gPr || !gPr->propRuUse || !r || r->editedList <= 0) return;
    if (!MenuRuUse) { MenuRuUse = new PopupMenu(); MenuRuUse->action = ActionUsePicked; }
    MenuRuUse->Clear();
    MenuRuUse->Add(T("The list of All"), 0, (int)IconType::list);
    MenuRuUse->Add(T("Its own list"), 1, (int)IconType::list);
    for (int m = 1; m < Routine::ModeN; m++)
        if (m != r->editedList) MenuRuUse->Add(std::string(T("Same as")) + " " + T(kModesUI[m]), 2 + m, (int)IconType::camera);
    PropsOpenMenuUnderButton(MenuRuUse, gPr->propRuUse->button);
}

// ---- Add (agrupado) / Remove / Move ----
static PopupMenu* MenuRuAdd = NULL;
static std::vector<PopupMenu*> gSubsAdd;
static void ActionAddPicked(int type) {
    Routine* r = RuActive(); if (!r || type < 0 || type >= StepN) return;
    std::vector<W3dStep>* L = RuList(r);
    // va DESPUES del paso elegido (o al final): el orden de la lista es el de ejecucion
    int pos = (r->activeStep >= 0 && r->activeStep < (int)L->size()) ? r->activeStep + 1 : (int)L->size();
    L->insert(L->begin() + pos, W3dNewStep(type));
    r->activeStep = pos;
    Change(r);
    PropsLayoutDirty();
}
static void ActionMenuAdd() {
    if (!gPr || !gPr->propRuRowAdd || gPr->propRuRowAdd->buttons.empty() || !RuActive()) return;
    if (!MenuRuAdd) { MenuRuAdd = new PopupMenu(); W3dRoutineMenuSteps(MenuRuAdd, gSubsAdd, ActionAddPicked, false); }
    PropsOpenMenuUnderButton(MenuRuAdd, gPr->propRuRowAdd->buttons[0]);
}
static void ActionRemove() {
    Routine* r = RuActive(); std::vector<W3dStep>* L = RuList(r);
    if (!r || !L || r->activeStep < 0 || r->activeStep >= (int)L->size()) return;
    extern void W3dAnimDeleteStep(Object*, unsigned);
    W3dAnimDeleteStep(r, (*L)[(size_t)r->activeStep].id);   // (sus curvas de animacion se van con el)
    L->erase(L->begin() + r->activeStep);
    if (r->activeStep >= (int)L->size()) r->activeStep = (int)L->size() - 1;
    Change(r);
    PropsLayoutDirty();
}
// el paso 'desde' pasa a quedar ANTES del que hoy esta en 'hasta' (hasta = cantidad: al final). Lo usan arrastrarlo
// con el mouse y agarrarlo con G
void PropsRoutineMoveStep(int from, int until) {
    Routine* r = RuActive(); std::vector<W3dStep>* L = RuList(r);
    if (!r || !L || from < 0 || from >= (int)L->size() || until < 0 || until > (int)L->size()) return;
    if (until == from || until == from + 1) { r->activeStep = from; return; }
    W3dStep p = (*L)[(size_t)from];
    L->erase(L->begin() + from);
    if (until > from) until--;
    L->insert(L->begin() + until, p);
    r->activeStep = until;
    Change(r);
    PropsLayoutDirty();
}
// el click en el CHECKBOX de la derecha de una fila: prende/apaga el paso (sin elegirlo)
bool PropsRoutineClickValue(int row) {
    Routine* r = RuActive(); std::vector<W3dStep>* L = RuList(r);
    if (!r || !L || row < 0 || row >= (int)L->size() || !W3dStepUsesOn((*L)[(size_t)row].type)) return false;
    (*L)[(size_t)row].on = !(*L)[(size_t)row].on;
    Change(r);
    return true;
}
// AGARRAR un paso (G; el 1 en el telefono): se mueve con el mouse o las flechas; click / Enter lo deja, Escape / click
// derecho lo devuelve a donde estaba
static int gGrab = -1, gGrabOrigin = -1, gGrabAccum = 0;
bool PropsRoutineGrabbing() { return gGrab >= 0 && RuActive() != NULL; }
void PropsRoutineGrab() {
    Routine* r = RuActive(); std::vector<W3dStep>* L = RuList(r);
    if (!r || !L || r->activeStep < 0 || r->activeStep >= (int)L->size()) return;
    gGrab = gGrabOrigin = r->activeStep; gGrabAccum = 0;
}
void PropsRoutineGrabMove(int rows) {   // + abajo, - arriba
    Routine* r = RuActive(); std::vector<W3dStep>* L = RuList(r);
    if (!PropsRoutineGrabbing() || !L) return;
    const int b = gGrab + rows;
    if (b < 0 || b >= (int)L->size()) return;
    PropsRoutineMoveStep(gGrab, rows > 0 ? b + 1 : b);
    gGrab = b;
}
void PropsRoutineGrabMouse(int dy) {      // el mouse se movio dy px (una fila cada alto de renglon)
    if (!PropsRoutineGrabbing()) return;
    gGrabAccum += dy;
    const int rowH = RowHeightGS + gapGS;
    while (gGrabAccum >= rowH) { gGrabAccum -= rowH; PropsRoutineGrabMove(1); }
    while (gGrabAccum <= -rowH) { gGrabAccum += rowH; PropsRoutineGrabMove(-1); }
}
void PropsRoutineGrabEnd(bool confirm) {
    if (!PropsRoutineGrabbing()) { gGrab = -1; return; }
    if (!confirm && gGrab != gGrabOrigin)
        PropsRoutineMoveStep(gGrab, gGrabOrigin > gGrab ? gGrabOrigin + 1 : gGrabOrigin);
    gGrab = -1;
}
// las TECLAS de la lista de pasos (con el panel activo): X / Supr borra el paso elegido, G lo agarra; agarrado, las
// flechas lo mueven, Enter lo deja y Escape lo devuelve. true = la tecla era de la lista
bool PropsRoutineKey(int key) {
    Routine* r = RuActive();
    if (!r || !gPr || !gPr->propRoutine || !gPr->propRoutine->visible) return false;
    if (PropsRoutineGrabbing()) {
        if (key == W3dK_UP)     { PropsRoutineGrabMove(-1); return true; }
        if (key == W3dK_DOWN)   { PropsRoutineGrabMove(+1); return true; }
        if (key == W3dK_RETURN) { PropsRoutineGrabEnd(true); return true; }
        if (key == W3dK_ESCAPE) { PropsRoutineGrabEnd(false); return true; }
        return true;   // (agarrado, nada mas)
    }
    if (r->activeStep < 0) return false;
    if (key == W3dK_X || key == W3dK_DELETE) { ActionRemove(); return true; }
    if (key == W3dK_G) { PropsRoutineGrab(); return true; }
    if (key == W3dK_I) {   // keyframe de los campos del paso en el frame actual (otra vez: los saca)
        W3dStep* p = RuStep(r);
        if (r->dirty) r->Resolve();   // (un paso recien agregado toma su id)
        extern void W3dAnimKeyStep(Object*, unsigned, int);
        extern int CurrentFrame;
        if (p) W3dAnimKeyStep(r, p->id, CurrentFrame);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  los DESPLEGABLES de referencias: un arbol de CARPETAS armado con menus de un pool (se reusan entre aperturas)
// ---------------------------------------------------------------------------
static std::vector<PopupMenu*> gPool;
static size_t gUsedPool = 0;
struct OptRef { std::string ref; int part; };
static std::vector<OptRef> gRefOpt;     // lo que va al paso por cada item (el id del item es su indice)
static bool gRefIsSecond = false;       // el menu abierto es el de 'ref2' (el objeto del test de visibilidad)
static void ActionPickedRef(int id);
static PopupMenu* NewMenu() {
    PopupMenu* m;
    if (gUsedPool < gPool.size()) m = gPool[gUsedPool];
    else { m = new PopupMenu(); gPool.push_back(m); }
    gUsedPool++;
    m->Clear(); m->title.clear();
    m->action = ActionPickedRef;
    return m;
}
static bool LowerNoCaps(const std::string& a, const std::string& b) {
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
        const int x = tolower((unsigned char)a[i]), y = tolower((unsigned char)b[i]);
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}
// el submenu de la carpeta "a/b/c" (creandolo, y a sus padres, la primera vez)
static PopupMenu* MenuFolder(PopupMenu* root, std::map<std::string, PopupMenu*>& folders, const std::string& folder) {
    if (folder.empty()) return root;
    std::map<std::string, PopupMenu*>::iterator it = folders.find(folder);
    if (it != folders.end()) return it->second;
    const size_t b = folder.find_last_of('/');
    PopupMenu* parent = MenuFolder(root, folders, b == std::string::npos ? std::string() : folder.substr(0, b));
    PopupMenu* m = NewMenu();
    parent->Add(b == std::string::npos ? folder : folder.substr(b + 1), 900000 + (int)folders.size(), (int)IconType::folder, m);
    folders[folder] = m;
    return m;
}
static int Option(const std::string& ref, int part) {
    OptRef o; o.ref = ref; o.part = part;
    gRefOpt.push_back(o);
    return (int)gRefOpt.size() - 1;
}
static bool MeshLower(const MeshResource* a, const MeshResource* b) {
    if (a->folder != b->folder) return LowerNoCaps(a->folder, b->folder);
    return LowerNoCaps(a->name, b->name);
}
// las MALLAS 3D por carpetas; con 'withParts', cada malla de varias partes abre las suyas
static void BuildMeshes(PopupMenu* root, const W3dStep& p, bool withParts) {
    std::vector<MeshResource*> ms;
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++) if (reg[i] && !reg[i]->deleted) ms.push_back(reg[i]);
    std::sort(ms.begin(), ms.end(), MeshLower);
    std::map<std::string, PopupMenu*> folders;
    for (size_t i = 0; i < ms.size(); i++) {
        MeshResource* r = ms[i];
        PopupMenu* m = MenuFolder(root, folders, r->folder);
        // las partes se conocen al cargarla: una que nunca se cargo se carga una vez (queda la lista de partes)
        if (withParts && r->parts.empty() && !r->Loaded()) { if (W3dMeshResourceRetain(r)) W3dMeshResourceRelease(r); }
        const int nParts = (int)r->parts.size();
        if (!withParts || nParts <= 1) {
            MenuItem* it = m->Add(r->name, Option(r->name, nParts == 1 ? 0 : -1), (int)IconType::mesh);
            it->green = (r->name == p.ref);
            continue;
        }
        PopupMenu* sub = NewMenu();
        MenuItem* it = m->Add(r->name, 800000 + (int)i, (int)IconType::mesh, sub);
        it->green = (r->name == p.ref);
        sub->Add(T("All parts"), Option(r->name, -1), (int)IconType::mesh)->green = (r->name == p.ref && p.integer < 0);
        for (int k = 0; k < nParts; k++) {
            char b[160];
            snprintf(b, sizeof b, "%d. %s (%d %s)", k, r->parts[(size_t)k].name.c_str(), r->parts[(size_t)k].count / 3, T("triangles"));
            sub->Add(b, Option(r->name, k), (int)IconType::material)->green = (r->name == p.ref && p.integer == k);
        }
    }
}
enum { kRefLoadTexture = 700000 };   // (el id de "Cargar textura" en el menu de las texturas)
static void BuildTextures(PopupMenu* root, const W3dStep& p) {
    // arriba, CARGAR una de afuera (una imagen 360 para el fondo, una textura que todavia no usa nadie)
    { MenuItem* it = root->Add(T("Load Texture"), kRefLoadTexture, (int)IconType::archive); (void)it; }
    std::vector<std::string> paths;
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i] && !Textures[i]->path.empty()) paths.push_back(Textures[i]->path);
    std::sort(paths.begin(), paths.end(), LowerNoCaps);
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    std::map<std::string, PopupMenu*> folders;
    for (size_t i = 0; i < paths.size(); i++) {
        const size_t b = paths[i].find_last_of("/\\");
        PopupMenu* m = MenuFolder(root, folders, b == std::string::npos ? std::string() : paths[i].substr(0, b));
        m->Add(Base(paths[i]), Option(paths[i], 0), (int)IconType::texture)->green = (paths[i] == p.ref);
    }
}
// los SHADERS (.glsl) del proyecto: los de la biblioteca (shaders/ adentro del .w3d, o de afuera)
static void BuildShaders(PopupMenu* root, const W3dStep& p) {
    std::vector<W3dResourceItem> its;
    W3dViewRecList(W3D_VIEW_SHADERS, its);
    for (size_t i = 0; i < its.size(); i++)
        root->Add(its[i].name, Option(its[i].id, 0), (int)IconType::script)->green = (its[i].id == p.ref);
}
static void Collect(Object* o, int rt, Routine* yo, std::vector<std::string>& v) {
    if (!o) return;
    bool ok = false;
    if (rt == RefRoutine) ok = o->getType() == ObjectType::routine && o != yo;
    else                 ok = o != yo && o->Parent != NULL;   // (la raiz no)
    if (ok) v.push_back(o->name);
    for (size_t i = 0; i < o->Childrens.size(); i++) Collect(o->Childrens[i], rt, yo, v);
}
static void BuildObjects(PopupMenu* root, Routine* r, int rt, const std::string& current, bool withNone) {
    std::vector<std::string> v;
    Object* top = r;
    while (top->Parent) top = top->Parent;
    Collect(top, rt, r, v);
    std::sort(v.begin(), v.end(), LowerNoCaps);
    v.erase(std::unique(v.begin(), v.end()), v.end());
    if (withNone) root->Add(T("(none)"), Option(std::string(), 0), -1)->green = current.empty();
    const int ico = rt == RefRoutine ? (int)IconType::list : (int)IconType::object;
    for (size_t i = 0; i < v.size(); i++) root->Add(v[i], Option(v[i], 0), ico)->green = (v[i] == current);
}
// "Cargar textura": la imagen elegida entra al proyecto y queda en el paso (la retiene la rutina al resolverlo)
static void RefLoadedTexture(const std::string& picked) {
    Routine* r = RuActive(); W3dStep* p = RuStep(r);
    if (!r || !p || W3dStepRef(p->type) != RefTexture) return;
    const std::string path = W3dLoadExternal(picked);   // (cargar de afuera = queda afuera, externo)
    Texture* t = TextureTake(path);
    if (!t) { extern void Notify(const std::string&, bool); Notify(T("The texture couldn't be loaded"), true); return; }
    p->ref = t->path;
    Change(r);
    r->Resolve();      // (la rutina toma su referencia antes de soltar la de la carga)
    TextureRelease(t);
    PropsLayoutDirty();
}
static void ActionPickedRef(int id) {
    Routine* r = RuActive(); W3dStep* p = RuStep(r);
    if (p && id == kRefLoadTexture && !gRefIsSecond) {
        OpenFileBrowser(T("Load Texture"), T("Open"), ".png .jpg .jpeg .bmp .tga .hdr", RefLoadedTexture);
        return;
    }
    if (!p || id < 0 || id >= (int)gRefOpt.size()) return;
    if (gRefIsSecond) p->ref2 = gRefOpt[(size_t)id].ref;
    else {
        p->ref = gRefOpt[(size_t)id].ref;
        if (UsesParts(p->type)) p->integer = gRefOpt[(size_t)id].part;
    }
    Change(r);
    PropsLayoutDirty();
}
static void OpenMenuRef(bool second) {
    Routine* r = RuActive(); W3dStep* p = RuStep(r);
    if (!gPr || !p) return;
    PropButton* button = second ? gPr->propRuRef2 : gPr->propRuRef;
    if (!button) return;
    gRefIsSecond = second;
    gUsedPool = 0;
    gRefOpt.clear();
    PopupMenu* root = NewMenu();
    const int rt = second ? RefObject : W3dStepRef(p->type);
    if (rt == RefMesh)        BuildMeshes(root, *p, UsesParts(p->type));
    else if (rt == RefTexture) BuildTextures(root, *p);
    else if (rt == RefShader)  BuildShaders(root, *p);
    else                       BuildObjects(root, r, rt, second ? p->ref2 : p->ref, second);
    if (root->items.empty()) root->Add(T("(nothing to choose)"), -1, -1);
    PropsOpenMenuUnderButton(root, button->button);
}
static void ActionMenuRef()  { OpenMenuRef(false); }
static void ActionMenuRef2() { OpenMenuRef(true); }

// ---- la LUZ (GL_LIGHT0..7) y la OPCION del paso (desplegables) ----
static PopupMenu* MenuRuLight = NULL;
static PopupMenu* MenuRuOpt   = NULL;
static void ActionPickedLight(int id) {
    Routine* r = RuActive(); W3dStep* p = RuStep(r);
    if (!p || id < 0 || id >= W3D_ROUTINE_LIGHTS) return;
    p->integer = id; Change(r);
}
static void ActionMenuLight() {
    W3dStep* p = RuStep(RuActive());
    if (!gPr || !gPr->propRuLight || !p) return;
    if (!MenuRuLight) { MenuRuLight = new PopupMenu(); MenuRuLight->action = ActionPickedLight; }
    MenuRuLight->Clear();
    for (int i = 0; i < W3D_ROUTINE_LIGHTS; i++) {
        char b[48]; snprintf(b, sizeof b, "%s %d (GL_LIGHT%d)", T("Light"), i, i);
        MenuRuLight->Add(b, i, (int)IconType::light)->green = (i == p->integer);
    }
    PropsOpenMenuUnderButton(MenuRuLight, gPr->propRuLight->button);
}
static void ActionPickedOpt(int id) {
    Routine* r = RuActive(); W3dStep* p = RuStep(r);
    if (!p || id < 0 || id >= W3dOptionsN(W3dStepOptions(p->type))) return;
    p->mode = id;
    if (p->type == StepDraw && id != RPrimTriangles) p->sub = RangePart;   // (el rango es de triangulos)
    Change(r);
    PropsLayoutDirty();
}
static void ActionMenuOption() {
    W3dStep* p = RuStep(RuActive());
    if (!gPr || !gPr->propRuOption || !p) return;
    const int l = W3dStepOptions(p->type);
    if (!MenuRuOpt) { MenuRuOpt = new PopupMenu(); MenuRuOpt->action = ActionPickedOpt; }
    MenuRuOpt->Clear();
    for (int i = 0; i < W3dOptionsN(l); i++)
        MenuRuOpt->Add(T(W3dOptionLabel(l, i)), i, W3dRoutineStepIcon(p->type))->green = (i == p->mode);
    PropsOpenMenuUnderButton(MenuRuOpt, gPr->propRuOption->button);
}
// "Max": el ultimo triangulo = el ultimo de la parte (aunque la malla cambie)
static void ActionMax() {
    Routine* r = RuActive(); W3dStep* p = RuStep(r);
    if (!p || p->type != StepDraw) return;
    p->n[1].ref.clear(); p->n[1].v = -1.0f;
    Change(r);
}

// (harness) abre un desplegable de la tarjeta: 0 Add, 1 la referencia, 2 la opcion, 3 la luz, 4 Render mode, 5 ref2
void PropsRoutineOpenMenu(int which) {
    switch (which) {
    case 0: ActionMenuAdd(); break;
    case 1: ActionMenuRef(); break;
    case 2: ActionMenuOption(); break;
    case 3: ActionMenuLight(); break;
    case 5: ActionMenuRef2(); break;
    default: ActionMenuList(); break;
    }
}

void PropsRoutineBuild(Properties* p) {
    if (!p) return;
    gPr = p;
    RoutineRowsCount = RuRows; RoutineRowText = RuRowText; RoutineRowSelect = RuRowSelect;
    RoutineRowIcon = RuRowIcon; RoutineRowValue = RuRowValue;
    p->propRoutine = new GroupPropertie(T("Routine"));
    p->propRoutine->valueWidth = 0.30f;
    p->propRuList = new PropButton(T("All"), IconType::camera);
    p->propRuList->button->dropdown = true; p->propRuList->withLabel = true; p->propRuList->name = T("Render mode");
    p->propRuList->action = ActionMenuList;
    p->propRoutine->properties.push_back(p->propRuList);
    // BUCLE: cuantas veces corre la lista seguida (la vuelta actual en @bucle[0])
    p->propRuRepeat = new PropFloat(T("Repeat"));
    p->propRuRepeat->SetRange(1.0f, 100000.0f); p->propRuRepeat->integer = true;
    p->propRuRepeat->fineStep = 1.0f; p->propRuRepeat->thickStep = 10.0f; p->propRuRepeat->dragStep = 0.2f;
    p->propRoutine->properties.push_back(p->propRuRepeat);
    p->propRuUse = new PropButton(T("The list of All"), IconType::list);
    p->propRuUse->button->dropdown = true; p->propRuUse->withLabel = true; p->propRuUse->name = T("Use");
    p->propRuUse->action = ActionMenuUse;
    p->propRoutine->properties.push_back(p->propRuUse);
    p->propRuSteps = new PropListMeshParts(T("Steps"));
    p->propRuSteps->mode = 14;
    p->propRuSteps->rowsMax = 8;
    p->propRoutine->properties.push_back(p->propRuSteps);
    p->propRuRowAdd = new PropButtonRow();
    Button* bAdd = p->propRuRowAdd->Add(T("Add"), ActionMenuAdd);
    bAdd->dropdown = true;
    p->propRuRowAdd->Add(T("Remove"), ActionRemove);
    p->propRoutine->properties.push_back(p->propRuRowAdd);
    // los campos del paso elegido (cada uno se ve solo si el paso lo usa)
    p->propRuRef = new PropButton("", IconType::mesh);
    p->propRuRef->button->dropdown = true; p->propRuRef->withLabel = true;
    p->propRuRef->action = ActionMenuRef;
    p->propRoutine->properties.push_back(p->propRuRef);
    for (int k = 0; k < 5; k++) {
        p->propRuInfo[k] = new PropLabel("", k >= 3);   // (las de los punteros pueden ser largas: con corte de linea)
        p->propRoutine->properties.push_back(p->propRuInfo[k]);
    }
    p->propRuRef2 = new PropButton("", IconType::object);
    p->propRuRef2->button->dropdown = true; p->propRuRef2->withLabel = true; p->propRuRef2->name = T("Object");
    p->propRuRef2->action = ActionMenuRef2;
    p->propRoutine->properties.push_back(p->propRuRef2);
    p->propRuText = new PropText(T("Function"), "");
    p->propRoutine->properties.push_back(p->propRuText);
    p->propRuLight = new PropButton("", IconType::light);
    p->propRuLight->button->dropdown = true; p->propRuLight->withLabel = true; p->propRuLight->name = T("Light");
    p->propRuLight->action = ActionMenuLight;
    p->propRoutine->properties.push_back(p->propRuLight);
    p->propRuOption = new PropButton("", IconType::list);
    p->propRuOption->button->dropdown = true; p->propRuOption->withLabel = true;
    p->propRuOption->action = ActionMenuOption;
    p->propRoutine->properties.push_back(p->propRuOption);
    p->propRuNotice = new PropLabel("", true);
    p->propRoutine->properties.push_back(p->propRuNotice);
    p->propRuOn = new PropBool(T("On"));
    p->propRoutine->properties.push_back(p->propRuOn);
    p->propRuColor = new PropColor(T("Color"));
    p->propRuColor->value = NULL;
    p->propRoutine->properties.push_back(p->propRuColor);
    p->propRuColorNum = new PropBool(T("Numbers / memories"));
    p->propRoutine->properties.push_back(p->propRuColorNum);
    p->propRuManual = new PropBool(T("Manual range"));
    p->propRoutine->properties.push_back(p->propRuManual);
    p->propRuArray = new PropText(T("Array"), "");
    p->propRoutine->properties.push_back(p->propRuArray);
    p->propRuInteger = new PropText(T("Value"), "");
    p->propRoutine->properties.push_back(p->propRuInteger);
    for (int k = 0; k < 5; k++) {
        p->propRuNum[k] = new PropText("", "");
        p->propRoutine->properties.push_back(p->propRuNum[k]);
    }
    p->propRuMax = new PropButtonRow();
    p->propRuMax->Add(T("Max (last triangle of the part)"), ActionMax);
    p->propRoutine->properties.push_back(p->propRuMax);
    static const char* const kBuffers[3] = { "Color buffer", "Depth buffer", "Stencil buffer" };
    for (int k = 0; k < 3; k++) {
        p->propRuBuffer[k] = new PropBool(T(kBuffers[k]));
        p->propRoutine->properties.push_back(p->propRuBuffer[k]);
    }
    p->propRuReason = new PropLabel("", true);
    p->propRuReason->hidden = true;
    p->propRuReason->red = true;
    p->propRoutine->properties.push_back(p->propRuReason);
    p->GroupProperties.push_back(p->propRoutine);
}

// un campo de texto con commit EN VIVO (lo tipeado entra al paso a cada tecla; sin foco muestra el valor real)
static bool Sync(PropText* pt, const std::string& current, std::string& last, std::string& typed) {
    if (!pt) return false;
    const bool focus = TextFieldLive(&pt->field);
    if (focus) {
        if (pt->field.text != last) { last = pt->field.text; typed = pt->field.text; return true; }
        return false;
    }
    last.clear();
    if (pt->field.text != current) { pt->field.SetText(current); g_redraw = true; }
    return false;
}
// un checkbox sobre algo que no es un bool: el PropBool apunta a 'espejo'. Si cambio desde el frame anterior fue un
// click (devuelve true y el valor nuevo queda en 'espejo'); si no, copia el valor real
static bool SyncBool(bool real, bool& mirror, bool& last) {
    if (mirror != last) { last = mirror; return true; }
    mirror = last = real;
    return false;
}
static bool IsMax(const std::string& s) {
    std::string t;
    for (size_t i = 0; i < s.size(); i++) if (!isspace((unsigned char)s[i])) t += (char)tolower((unsigned char)s[i]);
    return t == "max";
}
static const char* LabelInteger(int t) {
    switch (W3dStepInteger(t)) {
    case IntegerPart:  return "Part (-1 = all)";
    case IntegerCount: return "Steps to skip";
    case IntegerLayerUV: return "UV layer (0 = active)";
    default:           return "Value";
    }
}
static const char* LabelRef(int t, int rt) {
    if (t == StepDraw || t == StepVisible) return "Mesh part";
    switch (rt) {
    case RefMesh:   return "Mesh";
    case RefObject:  return "Object";
    case RefTexture: return "Texture";
    case RefShader:  return "Shader";
    case RefRoutine:  return "Routine";
    default:         return "Name";
    }
}
static int IconRef(int rt) {
    switch (rt) {
    case RefMesh:   return (int)IconType::mesh;
    case RefTexture: return (int)IconType::texture;
    case RefShader:  return (int)IconType::script;
    case RefRoutine:  return (int)IconType::list;
    default:         return (int)IconType::object;
    }
}
static void Row(PropLabel* l, const std::string& txt, unsigned& signature, unsigned bit) {
    l->hidden = txt.empty();
    if (txt.empty()) return;
    signature |= bit;
    if (l->name != txt) { l->name = txt; PropsLayoutDirty(); }
}

void PropsRoutineUpdate(Properties* p, bool visible) {
    if (!p || !p->propRoutine) return;
    Routine* r = visible ? RuActive() : NULL;
    p->propRoutine->visible = (r != NULL);
    if (p->propRuRepeat) p->propRuRepeat->value = r ? &r->repeat : NULL;
    if (!r) {
        if (p->propRuOn) p->propRuOn->value = NULL;
        if (p->propRuManual) p->propRuManual->value = NULL;
        for (int k = 0; k < 3; k++) if (p->propRuBuffer[k]) p->propRuBuffer[k]->value = NULL;
        if (p->propRuColor) p->propRuColor->value = NULL;
        if (p->propRuColorNum) p->propRuColorNum->value = NULL;
        return;
    }
    if (p->propRuSteps) { p->propRuSteps->obj = r; p->propRuSteps->selectIndex = r->activeStep; }
    p->propRuList->button->text = T(kModesUI[r->editedList]);
    p->propRuUse->hidden = (r->editedList == 0);
    if (r->editedList > 0) {
        const int u = r->use[r->editedList];
        std::string t = (u < 0) ? T("The list of All") : (u == r->editedList) ? T("Its own list")
                                : std::string(T("Same as")) + " " + T(kModesUI[u]);
        p->propRuUse->button->text = t;
    }
    std::vector<W3dStep>* L = RuList(r);
    if (p->propRuRowAdd && p->propRuRowAdd->buttons.size() >= 2)
        p->propRuRowAdd->buttons[1]->visible = (RuStep(r) != NULL);     // Remove: solo con un paso elegido
    W3dStep* step = RuStep(r);
    const int t = step ? step->type : -1;
    const bool draw = (t == StepDraw);
    const bool triangles = draw && step->mode == RPrimTriangles;
    const bool manual = triangles && step->sub == RangeManual;
    static std::string lastNum[5], lastEnt, lastArr, lastText;
    std::string typed;
    unsigned signature = 0;     // que campos se ven (si cambia, el layout se re-arma)

    // la referencia: un desplegable con lo que hay de ese tipo (las mallas y las texturas, por carpetas)
    const int rt = step ? W3dStepRef(t) : RefNothing;
    // ("Clear": la imagen, solo con el fondo personalizado)
    p->propRuRef->hidden = (rt == RefNothing || rt == RefFunction || (t == StepClear && step->mode != ClearBackgroundOwn));
    if (!p->propRuRef->hidden) {
        signature |= 1;
        p->propRuRef->name = T(t == StepClear ? "Image" : LabelRef(t, rt));
        p->propRuRef->button->icon = IconRef(rt);
        std::string txt = step->ref.empty() ? std::string(T("(choose)")) : ((rt == RefTexture || rt == RefShader) ? Base(step->ref) : step->ref);
        if (UsesParts(t) && !step->ref.empty()) {
            MeshResource* m = MeshOf(step);
            if (step->integer < 0) txt += std::string(" / ") + T("all");
            else if (m && step->integer < (int)m->parts.size()) txt += " / " + m->parts[(size_t)step->integer].name;
            else { char b[16]; snprintf(b, sizeof b, " / %d", step->integer); txt += b; }
        }
        p->propRuRef->button->text = txt;
    }
    // la info de la malla, en filas
    std::string fil[5];
    MeshResource* m = MeshOf(step);
    if (m) {
        char b[200];
        // una malla que solto su RAM (onlyGPU) se relee para armar un flujo que no estaba (una vez: queda en el cache)
        if (draw && m->onlyGPU && !W3dMeshFlowReady(m, step->integer, step->mode)) W3dMeshResourceEnsureRAM(m);
        if (UsesParts(t)) {
            snprintf(b, sizeof b, "%s: %d", T("Triangles"), W3dMeshFlowIndices(m, step->integer, RPrimTriangles) / 3); fil[0] = b;
            if (!draw) { snprintf(b, sizeof b, "%s: %d", T("Vertices"), m->vertexSize); fil[1] = b; }
            if (draw) {
                snprintf(b, sizeof b, "%s: %d (%s %d)", T("Indices"), W3dMeshFlowIndices(m, step->integer, step->mode),
                         T("list"), W3dMeshFlowIndices(m, step->integer, RPrimTriangles));
                fil[1] = b;   // (con que arrays se dibuja es estado: lo ponen los pasos de puntero, no este)
            }
        } else {
            snprintf(b, sizeof b, "%s: %d", T("Vertices"), m->vertexSize); fil[0] = b;
            const unsigned arr = t == StepPointerNormals ? W3DMD_NOR : t == StepPointerUV ? W3DMD_UV :
                                 t == StepPointerColors ? W3DMD_COL : W3DMD_POS;
            if (!W3dMeshResourceHasArray(m, arr)) fil[1] = T("This mesh does not have this array: do not draw with the array on");
        }
    }
    for (int k = 0; k < 5; k++) Row(p->propRuInfo[k], fil[k], signature, 1u << (16 + k));
    // el objeto del test de visibilidad (opcional) y, en "Llamar Lua", el objeto con el script y la funcion
    p->propRuRef2->hidden = (t != StepVisible && t != StepLua);
    if (!p->propRuRef2->hidden) {
        signature |= 1u << 23;
        p->propRuRef2->name = T(t == StepLua ? "Script of" : "Object");
        p->propRuRef2->button->text = step->ref2.empty() ? std::string(T(t == StepLua ? "(this routine)" : "(none)")) : step->ref2;
    }
    p->propRuText->hidden = (rt != RefFunction);
    if (!p->propRuText->hidden) {
        signature |= 1u << 24;
        if (Sync(p->propRuText, step->ref, lastText, typed)) { step->ref = typed; Change(r); }
    }
    // la luz
    p->propRuLight->hidden = !(step && W3dStepInteger(t) == IntegerLight);
    if (!p->propRuLight->hidden) {
        signature |= 4;
        char b[48]; snprintf(b, sizeof b, "%s %d", T("Light"), step->integer);
        p->propRuLight->button->text = b;
    }
    // la opcion (en "Dibujar elementos", la primitiva)
    const int opt = step ? W3dStepOptions(t) : OptNothing;
    p->propRuOption->hidden = (opt == OptNothing);
    if (opt != OptNothing) {
        signature |= 8;
        p->propRuOption->name = T(W3dOptionsTitle(opt));
        p->propRuOption->button->icon = W3dRoutineStepIcon(t);
        p->propRuOption->button->text = T(W3dOptionLabel(opt, step->mode));
    }
    // el aviso: primitiva simulada / el formato del array
    std::string notice;
    if (draw && W3dPrimSimulated(step->mode))
        notice = T("Simulated: OpenGL ES 1.1 has no quads or polygons. They are converted to triangles (more indices, slower).");
    else if (triangles && !manual)
        notice = T("Array: a memory @name[i] = count, then first/last triangle pairs (last -1 = max). Empty = the whole part.");
    Row(p->propRuNotice, notice, signature, 1u << 25);
    // on/off
    p->propRuOn->value = (step && W3dStepUsesOn(t)) ? &step->on : NULL;
    if (p->propRuOn->value) { signature |= 16; p->propRuOn->name = T(W3dStepLabelOn(t)); }
    // los COLORES: el selector de color (o los 4 numeros, para usar memorias)
    const bool isColor = step && W3dStepIsColor(t) && !ColorAuto(*step);   // ("Clear color" en Auto: sin color)
    static float colEsp[4], lastCol[4];
    static bool colHas = false;
    static bool numEsp = false, numLast = false, numReal = false;
    bool withNumbers = false;
    if (isColor) {
        bool memories = false;
        for (int k = 0; k < 4; k++) if (!step->n[k].ref.empty()) memories = true;
        if (memories) numReal = true;
        p->propRuColorNum->value = &numEsp;
        if (SyncBool(numReal, numEsp, numLast)) numReal = numEsp;
        withNumbers = numReal;
        signature |= 1u << 26;
        if (withNumbers) p->propRuColor->value = NULL;
        else {
            signature |= 1u << 27;
            p->propRuColor->value = colEsp;
            p->propRuColor->name = T(W3dStepLabel(t));
            if (colHas && memcmp(colEsp, lastCol, sizeof colEsp) != 0) {
                for (int k = 0; k < 4; k++) { step->n[k].ref.clear(); step->n[k].v = colEsp[k]; }
                Change(r);
            }
            for (int k = 0; k < 4; k++) colEsp[k] = lastCol[k] = step->n[k].v;
            colHas = true;
        }
    } else {
        p->propRuColor->value = NULL;
        p->propRuColorNum->value = NULL;
        colHas = false;
    }
    // Dibujar elementos: rango manual (checkbox) o array / la parte entera (solo con triangulos)
    static bool espManual = false, lastManual = false;
    p->propRuManual->value = triangles ? &espManual : NULL;
    if (triangles) {
        signature |= 32;
        if (SyncBool(manual, espManual, lastManual)) {
            if (espManual) {
                step->sub = RangeManual;
                if (step->n[0].ref.empty() && step->n[0].v < 0) step->n[0].v = 0;
            } else step->sub = step->n[2].ref.empty() ? RangePart : RangeArray;
            Change(r);
        }
    }
    p->propRuArray->hidden = !(triangles && !manual);
    if (!p->propRuArray->hidden) {
        signature |= 64;
        if (Sync(p->propRuArray, step->n[2].ref, lastArr, typed)) {
            W3dNum& a = step->n[2];
            std::string s = typed;
            while (!s.empty() && isspace((unsigned char)s[0])) s.erase(0, 1);
            if (s.empty()) { a.ref.clear(); step->sub = RangePart; }
            else { a.ref = s; step->sub = RangeArray; }
            Change(r);
        }
    }
    // el entero (la parte se elige en el desplegable de la malla; aca solo los pasos a saltear)
    const int eu = step ? W3dStepInteger(t) : IntegerNothing;
    p->propRuInteger->hidden = !(eu == IntegerCount || eu == IntegerLayerUV);
    if (!p->propRuInteger->hidden) {
        signature |= 128;
        p->propRuInteger->name = T(LabelInteger(t));
        char b[16]; snprintf(b, sizeof b, "%d", step->integer);
        if (Sync(p->propRuInteger, b, lastEnt, typed)) { step->integer = atoi(typed.c_str()); Change(r); }
    }
    // los numeros: un numero o "@memoria[i]" (el ultimo triangulo, ademas, "max"). Los colores, solo en modo numeros
    int nn = !step ? 0 : draw ? (manual ? 2 : 0) : W3dStepNumbers(*step);
    if ((isColor && !withNumbers) || (step && ColorAuto(*step))) nn = 0;
    for (int k = 0; k < 5; k++) {
        PropText* f = p->propRuNum[k];
        const bool uses = k < nn || (draw && step->sub == RangeArray && k == 3);   // (el array: sus banderas, opcional)
        f->hidden = !uses;
        if (!uses) continue;
        signature |= 256u << k;
        f->name = T(W3dStepNumberName(*step, k));
        W3dNum& n = step->n[k];
        const bool isLast = draw && k == 1;
        const std::string current = (isLast && n.ref.empty() && n.v < 0) ? std::string("max") : NumText(n);
        if (Sync(f, current, lastNum[k], typed)) {
            if (!typed.empty() && typed[0] == '@') n.ref = typed;
            else if (draw && k == 3) n.ref.clear();   // (sin banderas)
            else if (isLast && IsMax(typed)) { n.ref.clear(); n.v = -1.0f; }
            else { n.ref.clear(); n.v = (float)atof(typed.c_str()); }
            Change(r);
        }
    }
    // "Max" (manual)
    if (p->propRuMax) for (size_t b = 0; b < p->propRuMax->buttons.size(); b++) p->propRuMax->buttons[b]->visible = manual;
    if (manual) signature |= 1u << 13;
    // Clear: que buffers (la mascara de glClear)
    static bool espBuf[3] = { false, false, false }, lastBuf[3] = { false, false, false };
    for (int k = 0; k < 3; k++) {
        p->propRuBuffer[k]->value = (eu == IntegerBuffers) ? &espBuf[k] : NULL;
        if (eu != IntegerBuffers) continue;
        signature |= 1u << 14;
        if (SyncBool((step->integer & (1 << k)) != 0, espBuf[k], lastBuf[k])) {
            if (espBuf[k]) step->integer |= (1 << k); else step->integer &= ~(1 << k);
            Change(r);
        }
    }
    // el motivo (la misma validacion que corre antes de dibujarla)
    const bool ok = W3dRoutineValidate(r);
    p->propRuReason->hidden = ok;
    if (!ok) { signature |= 1u << 15; if (p->propRuReason->name != r->reason) { p->propRuReason->name = r->reason; PropsLayoutDirty(); } }
    static unsigned lastSignature = 0;
    if (signature != lastSignature) { lastSignature = signature; PropsLayoutDirty(); }
}
