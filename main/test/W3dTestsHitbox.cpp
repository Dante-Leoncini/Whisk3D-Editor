// ============================================================================
//  W3dTestsHitbox.cpp — comandos de harness del HITBOX y de los eventos lua.
//  Ver W3dTestsHitbox.h. Los llama W3dTestsResourcesCmd antes que los suyos.
//
//  Comandos:
//    hitboxnuevo <nombre> [padre N] [pos x y z] [rot x y z] [escala x y z] [tam x y z]
//                [centro x y z] [etiqueta S] [filtro S] [cuerpos 0|1] [activo 0|1] [visible 0|1]
//        crea un Hitbox ('-' = texto vacio). Sin padre cuelga de la escena.
//    hitboxinfo <nombre|*> [activo 0|1] [tam x y z] [centro x y z] [etiqueta S] [filtro S]
//               [cuerpos 0|1] [padre N|-] [duenio N] [tocando 0|1] [dentro N]
//        informa un hitbox ('*' = el activo) y asserta lo pedido (tolerancia 1e-4).
//    hitboxstats [hitboxes N] [cuerpos N] [pares N] [contactos N]
//        las cantidades del ULTIMO paso del motor (W3dHitboxGetStats).
//    hitboxadd [objeto|-]
//        Add > Hitbox del menu (la MISMA accion): con objeto, lo deja activo antes (el hitbox
//        nace hijo suyo, del tamano de sus mallas); con '-', sin activo (suelto en el cursor).
//    hitboxdup <nombre>
//        W3dDuplicateOne (el nucleo de Shift+D): la copia tiene los mismos campos y otro nombre.
//    hitboxprops <nombre>
//        la tarjeta "Hitbox" del primer panel de propiedades: visible y bindeada con el hitbox
//        activo, el texto de etiqueta/filtro con commit en vivo, el icono de la pestania y
//        el del outliner; con otro objeto activo la tarjeta se oculta y suelta los punteros.
//    compartidonum <clave> <min> [max]
//        el valor NUMERICO de getShared(clave) esta en [min, max] (max default = min).
//    hitboxpaso [n] / hitboxlimpiar
//        el motor de hitbox SOLO (sin Play ni scripts): n pasos / olvidar los pares.
//    nombreactivo <nombre>
//        renombra el objeto activo (el nombre de una primitiva depende del idioma).
//    posobjeto <nombre> x y z [tol]
//        la posicion LOCAL del objeto (tolerancia default 1e-3): lo que restaura el Stop.
//    hitboxgrilla <n> [grande 0|1] [cizalla 0|1] [maxms X]
//        escena NUEVA con n hitbox al azar (determinista: tamanos, rotaciones y escalas
//        distintas) y opcionalmente uno ENORME; un paso del motor contra la fuerza bruta
//        (SAT de todos los pares): tienen que dar los MISMOS pares. Informa ms del paso.
//        Con cizalla, uno de cada 5 cuelga de un vacio con escala NO uniforme y rotado (su
//        caja en mundo es un paralelepipedo): la esfera/grilla no tiene que perder pares.
//    hitboxsat <n> [semilla S]
//        el SAT (W3dObbOverlap) contra una referencia EXACTA independiente (aristas de cada
//        caja recortadas contra la otra en sus coordenadas afines) en n pares al azar de
//        cajas y PARALELEPIPEDOS (cizalla): ni falsos negativos ni falsos positivos.
//    hitboxvista <nombre> [overlays 0|1] [tipo 0|1] [queda 0|1] [dibuja 0|1] [pick 0|1] [ver 0|1]
//        lo que el viewport 3D VE y CLICKEA del hitbox: pone Show Overlays / Objects > Hitbox
//        del viewport (queda 1 = no los repone al final), dibuja y cuenta los pixeles del verde
//        del hitbox, hace un click (ScenePick3D) en el medio de una arista, y asserta: dibuja
//        (hay alambre), pick (el click lo elige) y ver (el overlay de debug showHitboxes).
//    ordenhijo <nombre> <indice>
//        mueve el objeto a esa posicion entre sus hermanos (lo que hace arrastrar en el
//        outliner): el orden del ARBOL deja de ser el de creacion (serial).
//    juegohitboxmin <carpeta>
//        el juego 3D minimo (juego3dmin) + hitbox, cuerpos rigidos y una animacion de escena,
//        con scripts que cuentan los eventos y un JUEZ que deja el veredicto en compartido
//        ("juez") y en el log, y pide quit() cuando termina.
//    juegohitboxlog <carpeta> <nombre>
//        corre el binario compilado y lee el veredicto del juez en su whisk3d.log.
//    fisicamallatest
//        cuerpos rigidos de tipo MALLA (W3dRigido + la grilla de W3dColMalla), sin Play: una caja
//        que cae sobre un plano malla queda apoyada; otra sobre una RAMPA de 30 grados baja sin
//        atravesarla; una lanzada contra una PARED malla no la cruza; physicsRay toca la malla; un
//        PERSONAJE que camina contra un escalon de 20 cm lo sube y uno de 60 cm lo frena.
// ============================================================================
#include "test/W3dTestsHitbox.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "objects/Objects.h"
#include "objects/Hitbox.h"
#include "objects/Empty.h"
#include "objects/ObjectMode.h"        // W3dDuplicateOne
#include "physics/W3dHitbox.h"
#include "physics/W3dRigid.h"
#include "physics/W3dColMesh.h"
#include "objects/Mesh.h"
#include "script/W3dScript.h"
#include "animation/Animation.h"       // la animacion de escena del juego de prueba
#include "config/W3dProfile.h"         // W3dNowMs
#include "ViewPorts/ViewPorts.h"       // rootViewport / ObjectIcon
#include "ViewPorts/Properties.h"
#include "WhiskUI/draw/icons.h"
#include "ViewPorts/ViewPort3D.h"     // hitboxvista: los flags de overlays del viewport
#include "ViewPorts/Pick3D.h"         // hitboxvista: el click de verdad (ScenePick3D)
#include "w3dGraphics.h"
#include "w3dFilesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>

extern void RestartScene();

// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
static Object* HbFind(const std::string& n) {
    if (n == "*") return ActiveObj;
    return SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
}
static std::string HbText(const std::string& s) { return s == "-" ? std::string() : s; }
static bool HbNear(float a, float b) { return fabsf(a - b) <= 1e-4f; }

static void HbNodes(ViewportBase* n, std::vector<ViewportBase*>& leaves) {
    if (!n) return;
    if (n->isLeaf()) { leaves.push_back(n); return; }
    if (n->ContainerKind() == 1) { HbNodes(((ViewportRow*)n)->childA, leaves); HbNodes(((ViewportRow*)n)->childB, leaves); }
    else { HbNodes(((ViewportColumn*)n)->childA, leaves); HbNodes(((ViewportColumn*)n)->childB, leaves); }
}
static ViewportBase* HbFirstLeaf(int kind) {
    std::vector<ViewportBase*> leaves;
    HbNodes(rootViewport, leaves);
    for (size_t i = 0; i < leaves.size(); i++) if (leaves[i]->ViewportKind() == kind) return leaves[i];
    return NULL;
}

static bool HbShared(const std::string& key, std::string* value) {
    const int nc = W3dSharedScriptCount();
    for (int i = 0; i < nc; i++) {
        std::string k, v;
        if (W3dSharedScriptPair(i, &k, &v) && k == key) { if (value) *value = v; return true; }
    }
    return false;
}

static bool HbWrite(const std::string& path, const char* text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fputs(text, f);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxnuevo
// ---------------------------------------------------------------------------
static bool CmdNewHitbox(std::istringstream& ss, std::string& err) {
    std::string name; ss >> name;
    if (name.empty()) { err = "hitboxnew: uso: hitboxnew <nombre> [clave valor]..."; return false; }
    Object* parent = NULL;
    Vector3 pos(0, 0, 0), rot(0, 0, 0), scale(1, 1, 1);
    float size[3] = { 1, 1, 1 }, center[3] = { 0, 0, 0 };
    std::string label, filter;
    int bodies = 0, active = 1, visible = 1;
    std::string key;
    while (ss >> key) {
        if (key == "parent") {
            std::string pn; ss >> pn;
            parent = HbFind(pn);
            if (!parent) { err = "hitboxnew: no existe el padre '" + pn + "'"; return false; }
        } else if (key == "pos" || key == "rot" || key == "scale" || key == "size" || key == "center") {
            float x = 0, y = 0, z = 0;
            if (!(ss >> x >> y >> z)) { err = "hitboxnew: '" + key + "' pide x y z"; return false; }
            if (key == "pos") pos = Vector3(x, y, z);
            else if (key == "rot") rot = Vector3(x, y, z);
            else if (key == "scale") scale = Vector3(x, y, z);
            else if (key == "size") { size[0] = x; size[1] = y; size[2] = z; }
            else { center[0] = x; center[1] = y; center[2] = z; }
        } else if (key == "label") { std::string v; ss >> v; label = HbText(v); }
        else if (key == "filter")     { std::string v; ss >> v; filter = HbText(v); }
        else if (key == "bodies")    ss >> bodies;
        else if (key == "active")     ss >> active;
        else if (key == "visible")    ss >> visible;
        else { err = "hitboxnew: clave desconocida '" + key + "'"; return false; }
    }
    Hitbox* h = new Hitbox(parent, pos);
    h->SetNameObj(name);
    h->SetRotEuler(rot);
    h->scale = scale;
    for (int k = 0; k < 3; k++) { h->size[k] = size[k]; h->center[k] = center[k]; }
    h->label = label; h->filter = filter;
    h->detectBodies = bodies != 0; h->active = active != 0; h->visible = visible != 0;
    DeselectAll(); ActiveObj = NULL;
    printf("      [hitboxnuevo] '%s' padre=%s tam=(%g,%g,%g)\n", h->name.c_str(),
           parent ? parent->name.c_str() : "(escena)", h->size[0], h->size[1], h->size[2]);
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxinfo
// ---------------------------------------------------------------------------
static bool CmdHitboxInfo(std::istringstream& ss, std::string& err) {
    std::string name; ss >> name;
    Object* o = HbFind(name);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxinfo: '" + name + "' no es un hitbox"; return false; }
    W3dHitboxBase* h = (W3dHitboxBase*)o;
    std::vector<Object*> inside;
    W3dHitboxInside(h, &inside);
    Object* du = W3dHitboxOwner(h);
    const bool parentIsScene = !h->Parent || h->Parent == SceneCollection;
    printf("      [hitboxinfo] '%s' activo=%d tam=(%g,%g,%g) centro=(%g,%g,%g) etiqueta='%s' filtro='%s' "
           "cuerpos=%d padre=%s duenio=%s tocando=%d dentro=%d\n",
           h->name.c_str(), h->active ? 1 : 0, h->size[0], h->size[1], h->size[2],
           h->center[0], h->center[1], h->center[2], h->label.c_str(), h->filter.c_str(),
           h->detectBodies ? 1 : 0, parentIsScene ? "-" : h->Parent->name.c_str(),
           du ? du->name.c_str() : "?", W3dHitboxTouchesSomething(h) ? 1 : 0, (int)inside.size());
    for (size_t i = 0; i < inside.size(); i++) printf("         adentro: %s\n", inside[i]->name.c_str());
    std::string key;
    while (ss >> key) {
        char b[256];
        if (key == "size" || key == "center") {
            float x = 0, y = 0, z = 0; ss >> x >> y >> z;
            const float* v = (key == "size") ? h->size : h->center;
            if (!HbNear(v[0], x) || !HbNear(v[1], y) || !HbNear(v[2], z)) {
                snprintf(b, sizeof(b), "hitboxinfo: %s = (%g,%g,%g), se esperaba (%g,%g,%g)", key.c_str(),
                         v[0], v[1], v[2], x, y, z);
                err = b; return false;
            }
        } else if (key == "active" || key == "bodies" || key == "touching" || key == "inside") {
            int v = 0; ss >> v;
            int real = (key == "active") ? (h->active ? 1 : 0) : (key == "bodies") ? (h->detectBodies ? 1 : 0)
                     : (key == "touching") ? (W3dHitboxTouchesSomething(h) ? 1 : 0) : (int)inside.size();
            if (real != v) { snprintf(b, sizeof(b), "hitboxinfo: %s = %d, se esperaba %d", key.c_str(), real, v); err = b; return false; }
        } else if (key == "label" || key == "filter") {
            std::string v; ss >> v; v = HbText(v);
            const std::string& real = (key == "label") ? h->label : h->filter;
            if (real != v) { err = "hitboxinfo: " + key + " = '" + real + "', se esperaba '" + v + "'"; return false; }
        } else if (key == "parent") {
            std::string v; ss >> v;
            const std::string real = parentIsScene ? std::string("-") : h->Parent->name;
            if (real != v) { err = "hitboxinfo: padre = '" + real + "', se esperaba '" + v + "'"; return false; }
        } else if (key == "owner") {
            std::string v; ss >> v;
            if (!du || du->name != v) { err = "hitboxinfo: duenio = '" + (du ? du->name : std::string("?")) + "', se esperaba '" + v + "'"; return false; }
        } else { err = "hitboxinfo: assert desconocido '" + key + "'"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxstats
// ---------------------------------------------------------------------------
static bool CmdHitboxStats(std::istringstream& ss, std::string& err) {
    const W3dHitboxStats st = W3dHitboxGetStats();
    printf("      [hitboxstats] hitboxes=%d cuerpos=%d probados=%d pares=%d contactos=%d\n",
           st.hitboxes, st.bodies, st.tested, st.pairs, st.contacts);
    std::string key;
    while (ss >> key) {
        int v = 0;
        if (!(ss >> v)) { err = "hitboxstats: falta el valor de '" + key + "'"; return false; }
        int real = -1;
        if (key == "hitboxes") real = st.hitboxes;
        else if (key == "bodies") real = st.bodies;
        else if (key == "pairs") real = st.pairs;
        else if (key == "contacts") real = st.contacts;
        else { err = "hitboxstats: assert desconocido '" + key + "'"; return false; }
        if (real != v) {
            char b[160]; snprintf(b, sizeof(b), "hitboxstats: %s = %d, se esperaba %d", key.c_str(), real, v);
            err = b; return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxadd / hitboxdup
// ---------------------------------------------------------------------------
static bool CmdHitboxAdd(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    DeselectAll();
    ActiveObj = NULL;
    if (!n.empty() && n != "-") {
        Object* o = HbFind(n);
        if (!o) { err = "hitboxadd: no existe '" + n + "'"; return false; }
        o->Select();
    }
    extern void AddHitbox();
    AddHitbox();
    Object* newValue = ActiveObj;
    if (!newValue || newValue->getType() != ObjectType::hitbox) { err = "hitboxadd: Add > Hitbox no dejo un hitbox activo"; return false; }
    W3dHitboxBase* h = (W3dHitboxBase*)newValue;
    printf("      [hitboxadd] '%s' padre=%s tam=(%g,%g,%g) centro=(%g,%g,%g)\n", h->name.c_str(),
           (h->Parent && h->Parent != SceneCollection) ? h->Parent->name.c_str() : "-",
           h->size[0], h->size[1], h->size[2], h->center[0], h->center[1], h->center[2]);
    return true;
}

static bool CmdHitboxDup(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = HbFind(n);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxdup: '" + n + "' no es un hitbox"; return false; }
    W3dHitboxBase* s = (W3dHitboxBase*)o;
    Object* d0 = W3dDuplicateOne(o);
    if (!d0 || d0->getType() != ObjectType::hitbox) { err = "hitboxdup: la copia no es un hitbox"; return false; }
    W3dHitboxBase* d = (W3dHitboxBase*)d0;
    bool equal = d->active == s->active && d->label == s->label && d->filter == s->filter &&
                 d->detectBodies == s->detectBodies && d->Parent == s->Parent &&
                 d->pos.x == s->pos.x && d->pos.y == s->pos.y && d->pos.z == s->pos.z &&
                 d->scale.x == s->scale.x && d->scale.y == s->scale.y && d->scale.z == s->scale.z;
    for (int k = 0; k < 3; k++) equal = equal && d->size[k] == s->size[k] && d->center[k] == s->center[k];
    const bool withScripts = (s->scriptData != NULL) == (d->scriptData != NULL);
    printf("      [hitboxdup] '%s' -> '%s' campos %s, scripts %s\n", s->name.c_str(), d->name.c_str(),
           equal ? "IGUALES" : "DISTINTOS", withScripts ? "copied" : "PERDIDOS");
    if (!equal || !withScripts) { err = "hitboxdup: la copia no tiene los mismos campos"; return false; }
    if (d->name == s->name) { err = "hitboxdup: la copia tiene el MISMO nombre"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxprops
// ---------------------------------------------------------------------------
static bool CmdHitboxProps(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = HbFind(n);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxprops: '" + n + "' no es un hitbox"; return false; }
    W3dHitboxBase* h = (W3dHitboxBase*)o;
    Properties* pr = (Properties*)HbFirstLeaf(3);
    if (!pr || !pr->propHitbox) { err = "hitboxprops: no hay panel de propiedades con la tarjeta Hitbox"; return false; }
    DeselectAll();
    o->Select();
    pr->activeTab = 2;
    pr->UpdateTabs();
    bool ok = pr->propHitbox->visible && pr->propHbActive->value == &h->active &&
              pr->propHbBodies->value == &h->detectBodies;
    static const int kAxis[3] = { 0, 2, 1 };   // los campos son Z-ARRIBA: "Y" = tam[2] (la z interna), "Z" = tam[1]
    for (int k = 0; k < 3; k++)
        ok = ok && pr->propHbSize[k]->value == &h->size[kAxis[k]] && pr->propHbCenter[k]->value == &h->center[kAxis[k]];
    const bool iconTab = pr->BarTabs.size() >= 3 && pr->BarTabs[2]->visible && pr->BarTabs[2]->icon == (int)IconType::hitbox;
    const bool iconOut = ObjectIcon(o) == (size_t)IconType::hitbox;
    // texto con commit EN VIVO: lo tipeado va al hitbox; sin foco el campo muestra el valor real
    const std::string label0 = h->label, filter0 = h->filter;
    g_activeTextField = &pr->propHbLabel->field;
    pr->propHbLabel->field.SetText("typed");
    pr->UpdateTabs();
    const bool live = (h->label == "typed");
    g_activeTextField = &pr->propHbFilter->field;
    pr->propHbFilter->field.SetText("other");
    pr->UpdateTabs();
    const bool liveFilter = (h->filter == "other");
    g_activeTextField = NULL;
    h->label = "desde lua";
    pr->UpdateTabs();
    const bool sample = (pr->propHbLabel->field.text == "desde lua");
    h->label = label0; h->filter = filter0;
    pr->UpdateTabs();
    // con OTRO objeto activo la tarjeta se oculta y suelta los punteros
    Empty* other = new Empty(NULL, Vector3(0, 0, 0));
    other->Select();
    pr->UpdateTabs();
    const bool loose = !pr->propHitbox->visible && pr->propHbActive->value == NULL && pr->propHbSize[0]->value == NULL;
    DeselectAll(); ActiveObj = NULL;
    std::vector<Object*>& hs = SceneCollection->Childrens;   // sacarlo de la escena ANTES de liberarlo
    for (size_t i = 0; i < hs.size(); i++) if (hs[i] == other) { hs.erase(hs.begin() + i); break; }
    delete other;
    printf("      [hitboxprops] tarjeta %s | texto en vivo %s/%s | muestra el valor real %s | icono pestania %s | "
           "icono outliner %s | otro activo la oculta %s\n",
           ok ? "BINDEADA" : "MAL", live ? "OK" : "MAL", liveFilter ? "OK" : "MAL", sample ? "OK" : "MAL",
           iconTab ? "OK" : "MAL", iconOut ? "OK" : "MAL", loose ? "OK" : "MAL");
    if (!ok || !live || !liveFilter || !sample || !iconTab || !iconOut || !loose) {
        err = "hitboxprops: la tarjeta Hitbox no se comporta (ver arriba)"; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  compartidonum
// ---------------------------------------------------------------------------
static bool CmdSharedNum(std::istringstream& ss, std::string& err) {
    std::string key; double mn = 0, mx = 0;
    ss >> key >> mn;
    if (!(ss >> mx)) mx = mn;
    std::string v;
    if (!HbShared(key, &v)) { err = "sharednum: no existe la clave '" + key + "'"; return false; }
    const double x = atof(v.c_str());
    printf("      [compartidonum] %s = %s (rango %g..%g)\n", key.c_str(), v.c_str(), mn, mx);
    if (x < mn - 1e-6 || x > mx + 1e-6) {
        char b[200]; snprintf(b, sizeof(b), "sharednum: %s = %s, fuera de [%g, %g]", key.c_str(), v.c_str(), mn, mx);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  posobjeto <nombre> x y z [tol]: la posicion LOCAL de un objeto (lo que restaura Stop)
// ---------------------------------------------------------------------------
static bool CmdPosObject(std::istringstream& ss, std::string& err) {
    std::string n; float x = 0, y = 0, z = 0, tol = 1e-3f;
    ss >> n >> x >> y >> z; ss >> tol;
    Object* o = HbFind(n);
    if (!o) { err = "posobject: no existe '" + n + "'"; return false; }
    const float d = sqrtf((o->pos.x - x) * (o->pos.x - x) + (o->pos.y - y) * (o->pos.y - y) + (o->pos.z - z) * (o->pos.z - z));
    printf("      [posobjeto] '%s' en (%.4f, %.4f, %.4f)\n", n.c_str(), o->pos.x, o->pos.y, o->pos.z);
    if (d > tol) {
        char b[200]; snprintf(b, sizeof(b), "posobject: '%s' esta a %.4f de (%g, %g, %g)", n.c_str(), d, x, y, z);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxpaso [n] / hitboxlimpiar: el motor SOLO (sin Play ni scripts), n pasos de 1/30 s
// ---------------------------------------------------------------------------
static bool CmdHitboxStep(std::istringstream& ss, std::string& err) {
    (void)err;
    int n = 1; ss >> n; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) W3dHitboxStep(1.0f / 30.0f);
    const W3dHitboxStats st = W3dHitboxGetStats();
    printf("      [hitboxpaso] %d paso(s): hitboxes=%d pares=%d\n", n, st.hitboxes, st.pairs);
    { extern bool g_redraw; g_redraw = true; }
    return true;
}

// ---------------------------------------------------------------------------
//  nombreactivo <nombre>: renombra el objeto activo (los nombres de las primitivas
//  dependen del idioma del sistema: "Cube"/"Cubo")
// ---------------------------------------------------------------------------
static bool CmdNameActive(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (!ActiveObj || n.empty()) { err = "nameactive: uso: nameactive <nombre> (con un objeto activo)"; return false; }
    ActiveObj->SetNameObj(n);
    if (ActiveObj->name != n) { err = "nameactive: el nombre '" + n + "' ya estaba usado"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxgrilla: el broadphase contra la fuerza bruta
// ---------------------------------------------------------------------------
struct HbBoxTest { Vector3 c, u[3], h; };
static HbBoxTest HbBoxWorld(W3dHitboxBase* hb) {
    HbBoxTest r;
    Matrix4 M; hb->GetWorldMatrix(M);
    const Vector3 col[3] = { Vector3(M.m[0], M.m[1], M.m[2]), Vector3(M.m[4], M.m[5], M.m[6]), Vector3(M.m[8], M.m[9], M.m[10]) };
    float hh[3];
    for (int k = 0; k < 3; k++) {
        const float len = col[k].Length();
        r.u[k] = col[k] * (1.0f / len);
        hh[k] = 0.5f * fabsf(hb->size[k]) * len;
        if (hh[k] < 1e-4f) hh[k] = 1e-4f;
    }
    r.h = Vector3(hh[0], hh[1], hh[2]);
    r.c = M * Vector3(hb->center[0], hb->center[1], hb->center[2]);
    return r;
}
static unsigned gHbSeed = 12345u;
static float HbRandom() { gHbSeed = gHbSeed * 1664525u + 1013904223u; return (float)((gHbSeed >> 8) & 0xFFFF) / 65535.0f; }

static bool CmdHitboxGrid(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 2) { err = "hitboxgrid: uso: hitboxgrid <n> [grande 0|1] [cizalla 0|1] [maxms X]"; return false; }
    int big = 0, shear = 0; double maxms = -1.0;
    std::string key;
    while (ss >> key) {
        if (key == "big") ss >> big;
        else if (key == "shear") ss >> shear;
        else if (key == "maxms") ss >> maxms;
        else { err = "hitboxgrid: clave desconocida '" + key + "'"; return false; }
    }
    RestartScene();
    gHbSeed = 12345u;
    const float side = 1.2f * cbrtf((float)n);
    std::vector<W3dHitboxBase*> hbs;
    int sheared = 0;
    W3dNamesLoading = true;   // nombres crudos (uniquificar uno a uno es O(n^2))
    for (int i = 0; i < n; i++) {
        const Vector3 p(HbRandom() * side, HbRandom() * side, HbRandom() * side);
        Hitbox* h = NULL;
        if (shear && i % 5 == 0) {
            // CIZALLA: un vacio con escala NO uniforme y rotado, y el hitbox rotado adentro: su
            // matriz de mundo no tiene columnas perpendiculares (la caja es un paralelepipedo)
            Empty* pa = new Empty(NULL, p);
            char np[32]; snprintf(np, sizeof(np), "Pa%d", i);
            pa->SetRawName(np);
            pa->SetRotEuler(Vector3(HbRandom() * 360.0f, HbRandom() * 360.0f, HbRandom() * 360.0f));
            pa->scale = Vector3(0.4f + 2.0f * HbRandom(), 0.4f + 2.0f * HbRandom(), 0.4f + 2.0f * HbRandom());
            h = new Hitbox(pa, Vector3(0, 0, 0));
            sheared++;
        } else {
            h = new Hitbox(NULL, p);
        }
        char nb[32]; snprintf(nb, sizeof(nb), "Hb%d", i);
        h->SetRawName(nb);
        for (int k = 0; k < 3; k++) h->size[k] = 0.2f + HbRandom();
        h->SetRotEuler(Vector3(HbRandom() * 360.0f, HbRandom() * 360.0f, HbRandom() * 360.0f));
        if (i % 7 == 0) h->scale = Vector3(1.5f, 0.5f, 1.0f);     // escala de mundo aplicada
        if (i % 11 == 0) { h->center[0] = 0.3f; h->center[2] = -0.2f; }
        hbs.push_back(h);
    }
    if (big) {
        Hitbox* g = new Hitbox(NULL, Vector3(side * 0.5f, side * 0.5f, side * 0.5f));
        g->SetRawName("Enorme");
        g->size[0] = side * 0.6f; g->size[1] = side * 0.4f; g->size[2] = side * 0.5f;
        g->SetRotEuler(Vector3(10.0f, 25.0f, 0.0f));
        hbs.push_back(g);
    }
    W3dNamesLoading = false;
    DeselectAll(); ActiveObj = NULL;
    // fuerza bruta: el SAT en TODOS los pares
    std::vector<HbBoxTest> boxes;
    for (size_t i = 0; i < hbs.size(); i++) boxes.push_back(HbBoxWorld(hbs[i]));
    const double tb0 = W3dNowMs();
    int raw = 0;
    for (size_t i = 0; i < boxes.size(); i++)
        for (size_t j = i + 1; j < boxes.size(); j++)
            if (W3dObbOverlap(boxes[i].c, boxes[i].u, boxes[i].h, boxes[j].c, boxes[j].u, boxes[j].h)) raw++;
    const double msRaw = W3dNowMs() - tb0;
    W3dHitboxClear();
    const double t0 = W3dNowMs();
    W3dHitboxStep(1.0f / 30.0f);
    const double ms = W3dNowMs() - t0;
    const W3dHitboxStats st = W3dHitboxGetStats();
    // segundo paso: los mismos pares (todos "siguen") y sin allocations nuevas
    const double t1 = W3dNowMs();
    W3dHitboxStep(1.0f / 30.0f);
    const double ms2 = W3dNowMs() - t1;
    const W3dHitboxStats st2 = W3dHitboxGetStats();
    printf("      [hitboxgrilla] n=%d%s%s | pares motor=%d (2do paso %d) fuerza bruta=%d | SAT probados=%d de %d pares posibles"
           " | paso %.2f ms (2do %.2f ms) | fuerza bruta %.2f ms\n",
           (int)hbs.size(), big ? " (+1 enorme)" : "", sheared ? " (con cizalla)" : "", st.pairs, st2.pairs, raw, st.tested,
           (int)(hbs.size() * (hbs.size() - 1) / 2), ms, ms2, msRaw);
    W3dHitboxClear();
    if (st.hitboxes != (int)hbs.size()) { err = "hitboxgrid: el motor no recolecto todos los hitbox"; return false; }
    if (st.pairs != raw || st2.pairs != raw) { err = "hitboxgrid: el broadphase PERDIO o INVENTO pares (no coincide con la fuerza bruta)"; return false; }
    if (maxms > 0.0 && ms2 > maxms) {
        char b[160]; snprintf(b, sizeof(b), "hitboxgrid: el paso tardo %.2f ms (tope %.2f)", ms2, maxms);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxsat: el SAT contra una referencia EXACTA e independiente
// ---------------------------------------------------------------------------
// una caja (o paralelepipedo) en doble precision: centro y semi-aristas (h_k * u_k)
struct HbBox { double c[3], e[3][3]; };   // e[k] = semiarista k (vector)

static HbBox HbPairOf(const Vector3& c, const Vector3* u, const Vector3& h, double f) {
    HbBox p;
    const double hh[3] = { h.x * f, h.y * f, h.z * f };
    p.c[0] = c.x; p.c[1] = c.y; p.c[2] = c.z;
    for (int k = 0; k < 3; k++) { p.e[k][0] = u[k].x * hh[k]; p.e[k][1] = u[k].y * hh[k]; p.e[k][2] = u[k].z * hh[k]; }
    return p;
}

// coordenadas AFINES de un punto en la caja q (q = cubo [-1,1]^3 en esas coordenadas)
static bool HbAffine(const HbBox& q, const double pw[3], double t[3]) {
    const double (*m)[3] = q.e;   // columnas = semiaristas
    // inversa de la matriz de columnas e0 e1 e2 por cofactores
    const double a = m[0][0], b = m[1][0], c = m[2][0];
    const double d = m[0][1], e = m[1][1], f = m[2][1];
    const double g = m[0][2], h = m[1][2], i = m[2][2];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (fabs(det) < 1e-12) return false;
    const double r[3] = { pw[0] - q.c[0], pw[1] - q.c[1], pw[2] - q.c[2] };
    t[0] = ((e * i - f * h) * r[0] + (c * h - b * i) * r[1] + (b * f - c * e) * r[2]) / det;
    t[1] = ((f * g - d * i) * r[0] + (a * i - c * g) * r[1] + (c * d - a * f) * r[2]) / det;
    t[2] = ((d * h - e * g) * r[0] + (b * g - a * h) * r[1] + (a * e - b * d) * r[2]) / det;
    return true;
}

// el segmento a-b (coordenadas afines) toca el cubo [-1,1]^3? (Liang-Barsky)
static bool HbSegmentInCube(const double a[3], const double b[3]) {
    double t0 = 0.0, t1 = 1.0;
    for (int k = 0; k < 3; k++) {
        const double d = b[k] - a[k];
        const double ps[2] = { -d, d };
        const double qs[2] = { a[k] + 1.0, 1.0 - a[k] };
        for (int s2 = 0; s2 < 2; s2++) {
            if (ps[s2] == 0.0) { if (qs[s2] < 0.0) return false; continue; }
            const double r = qs[s2] / ps[s2];
            if (ps[s2] < 0.0) { if (r > t0) t0 = r; } else { if (r < t1) t1 = r; }
            if (t0 > t1) return false;
        }
    }
    return true;
}

// alguna ARISTA de p toca el solido q? Dos poliedros convexos se tocan si y solo si alguna arista
// de uno toca al otro (todo vertice de la interseccion esta en una arista de alguno de los dos)
static bool HbEdgesTouch(const HbBox& p, const HbBox& q) {
    double v[8][3];
    for (int i = 0; i < 8; i++) {
        double w[3];
        for (int a = 0; a < 3; a++)
            w[a] = p.c[a] + ((i & 1) ? 1 : -1) * p.e[0][a] + ((i & 2) ? 1 : -1) * p.e[1][a] + ((i & 4) ? 1 : -1) * p.e[2][a];
        if (!HbAffine(q, w, v[i])) return false;
    }
    for (int i = 0; i < 8; i++)
        for (int bit = 1; bit < 8; bit <<= 1)
            if (!(i & bit) && HbSegmentInCube(v[i], v[i | bit])) return true;
    return false;
}

static bool HbReferenceTouch(const HbBox& p, const HbBox& q) {
    return HbEdgesTouch(p, q) || HbEdgesTouch(q, p);
}

// una rotacion al azar (tres angulos) como matriz 3x3 de filas
static void HbRotRandom(float r[3][3]) {
    const float a = HbRandom() * 6.2831853f, b = HbRandom() * 6.2831853f, c = HbRandom() * 6.2831853f;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b), cc = cosf(c), sc = sinf(c);
    // Rz(c) * Ry(b) * Rx(a)
    r[0][0] = cc * cb; r[0][1] = cc * sb * sa - sc * ca; r[0][2] = cc * sb * ca + sc * sa;
    r[1][0] = sc * cb; r[1][1] = sc * sb * sa + cc * ca; r[1][2] = sc * sb * ca - cc * sa;
    r[2][0] = -sb;     r[2][1] = cb * sa;                r[2][2] = cb * ca;
}

// una caja al azar como la arma el motor: matriz de mundo R1 * S * R2 (un padre rotado con escala
// no uniforme y un hijo rotado); columnas normalizadas = ejes, largo * tam / 2 = semiejes
static void HbBoxRandom(bool shear, float side, Vector3* c, Vector3 u[3], Vector3* h) {
    float r1[3][3], r2[3][3], m[3][3];
    HbRotRandom(r1); HbRotRandom(r2);
    const float s[3] = { shear ? 0.3f + 2.7f * HbRandom() : 1.0f,
                         shear ? 0.3f + 2.7f * HbRandom() : 1.0f,
                         shear ? 0.3f + 2.7f * HbRandom() : 1.0f };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float acc = 0.0f;
            for (int k = 0; k < 3; k++) acc += r1[i][k] * s[k] * r2[k][j];
            m[i][j] = acc;
        }
    float hh[3];
    for (int k = 0; k < 3; k++) {
        Vector3 col(m[0][k], m[1][k], m[2][k]);
        const float len = col.Length();
        u[k] = col * (1.0f / len);
        hh[k] = 0.5f * (0.2f + 1.3f * HbRandom()) * len;
    }
    *h = Vector3(hh[0], hh[1], hh[2]);
    *c = Vector3((HbRandom() - 0.5f) * side, (HbRandom() - 0.5f) * side, (HbRandom() - 0.5f) * side);
}

static bool CmdHitboxSat(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 1) { err = "hitboxsat: uso: hitboxsat <n> [semilla S]"; return false; }
    unsigned seed = 777u;
    std::string key;
    while (ss >> key) {
        if (key == "seed") ss >> seed;
        else { err = "hitboxsat: clave desconocida '" + key + "'"; return false; }
    }
    gHbSeed = seed;
    int touch = 0, notTouch = 0, doubtful = 0, falseNeg = 0, falsePos = 0, withShear = 0;
    for (int i = 0; i < n; i++) {
        // un tercio cajas de verdad contra cajas, el resto con al menos un paralelepipedo
        const bool cA = (i % 3) != 0, cB = (i % 3) == 2;
        Vector3 ca, ua[3], ha, cb, ub[3], hb;
        HbBoxRandom(cA, 3.0f, &ca, ua, &ha);
        HbBoxRandom(cB, 3.0f, &cb, ub, &hb);
        if (cA || cB) withShear++;
        const bool sat = W3dObbOverlap(ca, ua, ha, cb, ub, hb);
        // la referencia con las cajas un poco MAS CHICAS (si tocan, seguro se solapan) y un poco
        // MAS GRANDES (si no tocan, seguro no): lo que queda entre las dos es un roce, no se juzga
        const bool safeYes = HbReferenceTouch(HbPairOf(ca, ua, ha, 0.999), HbPairOf(cb, ub, hb, 0.999));
        const bool maybe = HbReferenceTouch(HbPairOf(ca, ua, ha, 1.001), HbPairOf(cb, ub, hb, 1.001));
        if (safeYes) { touch++; if (!sat) falseNeg++; }
        else if (!maybe) { notTouch++; if (sat) falsePos++; }
        else doubtful++;
    }
    printf("      [hitboxsat] %d pares (%d con cizalla): se tocan %d, no %d, roce %d | falsos negativos %d, "
           "falsos positivos %d\n", n, withShear, touch, notTouch, doubtful, falseNeg, falsePos);
    if (falseNeg || falsePos) { err = "hitboxsat: el SAT no coincide con la referencia exacta"; return false; }
    if (touch < n / 10 || notTouch < n / 10) { err = "hitboxsat: la muestra no tiene casos de los dos lados"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxvista: lo que el viewport 3D VE y CLICKEA de un hitbox
// ---------------------------------------------------------------------------
static bool CmdHitboxView(std::istringstream& ss, std::string& err) {
    std::string name; ss >> name;
    Object* o = HbFind(name);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxview: '" + name + "' no es un hitbox"; return false; }
    W3dHitboxBase* hb = (W3dHitboxBase*)o;
    int ovl = -1, type = -1, remains = 0, espDraws = -1, espPick = -1, espShow = -1;
    std::string key;
    while (ss >> key) {
        int v = 0;
        if (!(ss >> v)) { err = "hitboxview: falta el valor de '" + key + "'"; return false; }
        if (key == "overlays") ovl = v;
        else if (key == "type") type = v;
        else if (key == "remains") remains = v;
        else if (key == "draws") espDraws = v;
        else if (key == "pick") espPick = v;
        else if (key == "show") espShow = v;
        else { err = "hitboxview: clave desconocida '" + key + "'"; return false; }
    }
    namespace gfx = w3dEngine;
    extern Viewport3D* Viewport3DActive;
    extern int W3dScreenHeight;
    if (!rootViewport) { err = "hitboxview: no hay layout de viewports"; return false; }
    DeselectAll(); ActiveObj = NULL;   // seleccionado cambia el color del alambre
    { extern void LoadPendingTextures(); LoadPendingTextures(); }
    rootViewport->Render();                   // puebla Viewport3DActive
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { err = "hitboxview: no hay viewport 3D"; return false; }
    const bool ovl0 = vp->showOverlays, type0 = vp->showHitbox;
    if (ovl >= 0) vp->showOverlays = (ovl != 0);
    if (type >= 0) vp->showHitbox = (type != 0);
    rootViewport->Render();                   // publica los flags (w3dRenderOverlays, g_showHitbox)
    vp->Render();                             // el render REAL de la vista
    gfx::Finish();
    // el alambre: pixeles del VERDE del hitbox activo (0.25, 0.90, 0.35)
    const int vpGLY = W3dScreenHeight - vp->y - vp->height;
    std::vector<unsigned char> buf((size_t)vp->width * vp->height * 4);
    gfx::ReadPixelsRGBA(vp->x, vpGLY, vp->width, vp->height, &buf[0]);
    int green = 0;
    for (size_t i = 0; i + 3 < buf.size(); i += 4)
        if (abs((int)buf[i] - 64) <= 10 && abs((int)buf[i + 1] - 229) <= 10 && abs((int)buf[i + 2] - 89) <= 10) green++;
    const bool draws = green >= 20;
    // el click: en el MEDIO de una arista de la caja (la de arriba-adelante, a lo largo de X)
    Matrix4 M; hb->GetWorldMatrix(M);
    const Vector3 middle = M * Vector3(hb->center[0], hb->center[1] + 0.5f * fabsf(hb->size[1]),
                                      hb->center[2] + 0.5f * fabsf(hb->size[2]));
    vp->BindView();
    float sx = 0, sy = 0;
    bool pick = false;
    const bool inView = vp->ProjectPoint(middle, sx, sy) && sx >= 0 && sy >= 0 &&
                         sx < (float)vp->width && sy < (float)vp->height;
    if (inView) {
        ScenePick3D(vp->x + (int)sx, vp->y + (int)sy, vp->x, vp->y, vp->width, vp->height, W3dScreenHeight);
        pick = hb->select;
        DeselectAll(); ActiveObj = NULL;
    }
    if (!remains) { vp->showOverlays = ovl0; vp->showHitbox = type0; }
    rootViewport->Render();                   // deja los flags publicados como quedo el viewport
    printf("      [hitboxvista] '%s' overlays=%d tipo=%d | pixeles del alambre=%d (%s) | click en (%d,%d) %s | "
           "verHitboxes=%d\n", hb->name.c_str(), (ovl >= 0 ? ovl : (int)ovl0), (type >= 0 ? type : (int)type0),
           green, draws ? "SE VE" : "no se ve", (int)sx, (int)sy,
           !inView ? "FUERA DE LA VISTA" : (pick ? "LO ELIGE" : "no lo elige"), g_w3dHitboxVer ? 1 : 0);
    char b[200];
    if (espDraws >= 0 && (draws ? 1 : 0) != espDraws) {
        snprintf(b, sizeof(b), "hitboxview: dibuja = %d (%d pixeles), se esperaba %d", draws ? 1 : 0, green, espDraws);
        err = b; return false;
    }
    if (espPick >= 0 && !inView) { err = "hitboxview: la arista queda fuera de la vista"; return false; }
    if (espPick >= 0 && (pick ? 1 : 0) != espPick) {
        snprintf(b, sizeof(b), "hitboxview: pick = %d, se esperaba %d", pick ? 1 : 0, espPick);
        err = b; return false;
    }
    if (espShow >= 0 && (g_w3dHitboxVer ? 1 : 0) != espShow) {
        snprintf(b, sizeof(b), "hitboxview: verHitboxes = %d, se esperaba %d", g_w3dHitboxVer ? 1 : 0, espShow);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  ordenhijo <nombre> <indice>: reordena entre hermanos (como arrastrar en el outliner)
// ---------------------------------------------------------------------------
static bool CmdOrderChild(std::istringstream& ss, std::string& err) {
    std::string n; int idx = -1;
    ss >> n >> idx;
    Object* o = HbFind(n);
    if (!o || idx < 0) { err = "childorder: uso: childorder <nombre> <indice>"; return false; }
    Object* parent = o->Parent ? o->Parent : SceneCollection;
    std::vector<Object*>& h = parent->Childrens;
    size_t i = 0;
    while (i < h.size() && h[i] != o) i++;
    if (i == h.size()) { err = "childorder: '" + n + "' no esta entre los hijos de su padre"; return false; }
    if ((size_t)idx >= h.size()) { err = "childorder: indice fuera de rango"; return false; }
    h.erase(h.begin() + i);
    h.insert(h.begin() + idx, o);
    printf("      [ordenhijo] '%s' pasa a la posicion %d de '%s' (serial %u)\n", o->name.c_str(), idx,
           parent->name.c_str(), o->serial);
    return true;
}

// ---------------------------------------------------------------------------
//  juegohitboxmin / juegohitboxlog: lo mismo en el Play y en el juego compilado
// ---------------------------------------------------------------------------
// el contador generico: alEntrar/alQuedarse/alSalir (y alTocar) en getShared("<prefijo>_..."); de alTocar anota la
// vertical de la normal (nz: lua es Z arriba) en "<prefijo>_nz"
static const char* kLuaCounter =
    "-- contador de eventos de hitbox / contactos (prueba del motor)\n"
    "properties = { prefijo = \"x\" }\n"
    "local p = \"x\"\n"
    "local function sum(c) local k = p .. \"_\" .. c; setShared(k, (getShared(k) or 0) + 1) end\n"
    "local function name(o) if o then return getName(o) end return \"nil\" end\n"
    "function start()\n"
    "  p = property(\"prefijo\")\n"
    "  setShared(p .. \"_entrar\", 0); setShared(p .. \"_quedarse\", 0)\n"
    "  setShared(p .. \"_salir\", 0);  setShared(p .. \"_tocar\", 0)\n"
    "end\n"
    "function onEnter(other, mine, own)\n"
    "  sum(\"entrar\")\n"
    "  setShared(p .. \"_otro\", name(other)); setShared(p .. \"_mio\", name(mine)); setShared(p .. \"_suyo\", name(own))\n"
    "end\n"
    "function onStay(other, mine, own) sum(\"quedarse\") end\n"
    "function onExit(other, mine, own) sum(\"salir\"); setShared(p .. \"_salio\", name(other)) end\n"
    "function onTouch(other, impulse, x, y, z, nx, ny, nz)\n"
    "  sum(\"tocar\"); setShared(p .. \"_tocado\", name(other)); setShared(p .. \"_nz\", nz)\n"
    "end\n";
// un objeto que avanza en X un paso FIJO por tick (determinista aunque el dt varie). La propiedad "y" es la ALTURA
// (asi se llamaba cuando lua era Y arriba; el Movil la trae en su ref): con Z arriba va en la z de setPosition
static const char* kLuaMove =
    "-- avanza en X un paso fijo por tick (x = x0 + paso * n)\n"
    "properties = { x0 = 0, paso = 0.4, y = 0 }\n"
    "local n = 0\n"
    "function update(dt)\n"
    "  n = n + 1\n"
    "  setPosition(self(), property(\"x0\") + property(\"paso\") * n, 0, property(\"y\"))\n"
    "end\n";
// la caja que cae: los dinamicos arrancan DORMIDOS, se la despierta con una velocidad
static const char* kLuaWake =
    "-- despierta el cuerpo rigido (los dinamicos arrancan dormidos)\n"
    "function start() physicsVel(self(), 0, 0, -0.5) end   -- hacia abajo (Z arriba)\n";
// arranca la animacion de escena (one-shot)
static const char* kLuaAnim =
    "function start() animScene(\"Subir\", false) end\n";
// EL JUEZ: mira todo y deja el veredicto (compartido + log); en el juego pide quit(). Los
// contadores tienen que dar EXACTO (un alTocar por lado, como en el Play) y, una vez que se
// cumple todo, se sigue mirando 1 s MAS (mucho mas que la gracia de 0,15 s de los contactos): un
// segundo golpe falso (dt variable del juego compilado) o cualquier otro evento de mas es FALTA.
static const char* kLuaJudge =
    "-- veredicto de la prueba de hitbox del juego compilado\n"
    "local t, seg = 0, 0\n"
    "local ready = false\n"
    "local okFrom = nil\n"
    "local function c(k) return getShared(k) or -1 end\n"
    "function update(dt)\n"
    "  if ready then return end\n"
    "  t = t + 1; seg = seg + dt\n"
    "  local _, _, ay = getPosition(find(\"Ascensor\"))   -- la altura: la z (Z arriba)\n"
    "  local ok = c(\"puerta_entrar\") == 1 and c(\"puerta_quedarse\") == 7 and c(\"puerta_salir\") == 1\n"
    "     and c(\"zona_entrar\") == 1 and c(\"zona_salir\") == 1 and c(\"caja_entrar\") == 1 and c(\"caja_salir\") == 1\n"
    "     and c(\"piso_tocar\") == 1 and c(\"caja_tocar\") == 1 and ay > 4.9\n"
    "  if ok and not okFrom then okFrom = seg end\n"
    "  local fin = (okFrom and (not ok or seg - okFrom >= 1.0)) or seg > 20 or t > 100000\n"
    "  if fin then\n"
    "    ok = ok and okFrom ~= nil\n"
    "    ready = true\n"
    "    local line = string.format(\"[juegohitbox] %s puerta=%d/%d/%d zona=%d/%d caja=%d/%d tocar=%d/%d ascensor=%.2f ticks=%d\",\n"
    "      ok and \"OK\" or \"FALTA\", c(\"puerta_entrar\"), c(\"puerta_quedarse\"), c(\"puerta_salir\"),\n"
    "      c(\"zona_entrar\"), c(\"zona_salir\"), c(\"caja_entrar\"), c(\"caja_salir\"), c(\"piso_tocar\"), c(\"caja_tocar\"), ay, t)\n"
    "    info(line)\n"
    "    setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    quit()\n"
    "  end\n"
    "end\n";

static void HbScript(Object* o, const char* path, const char* prop, const char* value) {
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    W3dScriptEntry e; e.path = path;
    if (prop) e.refs.push_back(std::make_pair(std::string(prop), std::string(value)));
    o->scriptData->scripts.push_back(e);
}
static void HbRef(Object* o, const char* prop, const char* value) {
    o->scriptData->scripts.back().refs.push_back(std::make_pair(std::string(prop), std::string(value)));
}
static Empty* HbEmpty(const char* name, const Vector3& pos) {
    Empty* e = new Empty(NULL, pos);
    e->SetNameObj(name);
    return e;
}

// un plano (2x2, la primitiva) escalado y girado, con fisica de tipo malla
static Object* FmPlane(const char* name, const Vector3& pos, float scale, const Vector3& axis, float degrees) {
    Object* o = NewMesh(MeshType(MeshType::plane), NULL, false);
    if (!o) return NULL;
    o->SetNameObj(name);
    o->pos = pos;
    o->scale = Vector3(scale, scale, scale);
    if (degrees != 0.0f) o->SetRotAxisAngle(axis, degrees);
    o->physics = new W3dRigidDef();
    o->physics->type = 3;
    return o;
}

static Empty* FmBox(const char* name, const Vector3& pos) {
    Empty* e = HbEmpty(name, pos);
    e->physics = new W3dRigidDef();   // dinamica, caja de 1 m
    e->physics->friction = 0.2f;
    return e;
}

static bool CmdPhysicsMeshTest(std::istringstream&, std::string& err) {
    W3dRigidClear();
    W3dColClear();
    // piso plano en y=0, rampa de 30 grados (sube hacia -z) y una pared vertical en x=60
    Object* floor   = FmPlane("FmPiso",   Vector3(0, 0, 0),  10.0f, Vector3(1, 0, 0), 0.0f);
    Object* ramp    = FmPlane("FmRampa",  Vector3(30, 0, 0), 10.0f, Vector3(1, 0, 0), 30.0f);
    Object* wall    = FmPlane("FmPared",  Vector3(60, 0, 0), 10.0f, Vector3(0, 0, 1), 90.0f);
    Object* groundP = FmPlane("FmSueloP", Vector3(55, 0, 0), 10.0f, Vector3(1, 0, 0), 0.0f);
    if (!floor || !ramp || !wall || !groundP) { err = "physicsmeshtest: NewMesh fallo"; return false; }
    Empty* cFloor = FmBox("FmCajaPiso",  Vector3(0, 3, 0));
    Empty* cRamp  = FmBox("FmCajaRampa", Vector3(30, 3, 0));
    Empty* cWall  = FmBox("FmCajaPared", Vector3(55, 0.6f, 0));
    // escalones: un cubo malla (2 m de lado) aplastado a 20 cm y otro a 60 cm, sobre un piso en x=90 / z=+-20
    Object* groundE = FmPlane("FmSueloE", Vector3(90, 0, 0), 30.0f, Vector3(1, 0, 0), 0.0f);
    Object* under  = NewMesh(MeshType(MeshType::cube), NULL, false);
    Object* height = NewMesh(MeshType(MeshType::cube), NULL, false);
    if (!groundE || !under || !height) { err = "physicsmeshtest: NewMesh fallo"; return false; }
    under->SetNameObj("FmEscalon"); under->pos = Vector3(92, 0.1f, -10); under->scale = Vector3(2, 0.1f, 2);
    height->SetNameObj("FmMuro");    height->pos = Vector3(92, 0.3f, 10);  height->scale = Vector3(2, 0.3f, 2);
    under->physics  = new W3dRigidDef(); under->physics->type = 3;
    height->physics = new W3dRigidDef(); height->physics->type = 3;
    Empty* pj1 = HbEmpty("FmPj1", Vector3(87, 0.01f, -10));
    Empty* pj2 = HbEmpty("FmPj2", Vector3(87, 0.01f, 10));
    Empty* pjs[2] = { pj1, pj2 };
    for (int k = 0; k < 2; k++) {
        W3dRigidDef* d = new W3dRigidDef();
        d->type = 2; d->mass = 70; d->box[0] = 0.7f; d->box[1] = 1.75f; d->box[2] = 0.7f; d->center[1] = 0.875f;
        pjs[k]->physics = d;
    }
    W3dRigidEnsure(SceneCollection);
    W3dRigidSetVel(cFloor, Vector3(0, -0.1f, 0));
    W3dRigidSetVel(cRamp, Vector3(0, -0.1f, 0));
    W3dRigidSetVel(cWall, Vector3(12, 0, 0));
    float minDistRamp = 1e30f, yStep = -1.0f;
    // la rampa: su normal y su eje a lo largo (la del plano Y-up y su Z, giradas). La distancia al plano se mide
    // mientras la caja esta ENCIMA (la rampa mide 20 m: en 4 s se desliza hasta el borde y cae)
    const Vector3 nr = ramp->Rot() * Vector3(0, 1, 0), lr = ramp->Rot() * Vector3(0, 0, 1);
    for (int i = 0; i < 120; i++) {
        // los personajes caminan hacia +x (el script les fija la velocidad horizontal en cada cuadro, como el juego)
        for (int k = 0; k < 2; k++) {
            Vector3 v; W3dRigidVel(pjs[k], &v);
            W3dRigidSetVel(pjs[k], Vector3(2.0f, v.y, 0.0f));
        }
        W3dRigidStep(1.0f / 30.0f);
        if (pj1->pos.x > 91.0f && pj1->pos.x < 93.0f && pj1->pos.y > yStep) yStep = pj1->pos.y;
        if (fabsf(lr.Dot(cRamp->pos - ramp->pos)) > 9.0f) continue;
        const float d = nr.Dot(cRamp->pos - ramp->pos);
        if (d < minDistRamp) minDistRamp = d;
    }
    bool ok = true;
    char buf[512];
    // 1) apoyada en el piso: el centro de la caja de 1 m a 0.5 (el slop de la penetracion tolera 1-2 cm)
    snprintf(buf, sizeof(buf), "      [fisicamallatest] piso y=%.3f | rampa: min dist al plano %.3f, z=%.2f | pared x=%.3f\n",
             cFloor->pos.y, minDistRamp, cRamp->pos.z, cWall->pos.x);
    printf("%s", buf);
    if (fabsf(cFloor->pos.y - 0.5f) > 0.05f) { err = "physicsmeshtest: la caja no quedo apoyada en el piso malla"; ok = false; }
    // 2) la rampa: nunca la atraveso (el centro siempre a ~0.5 o mas del plano) y bajo por ella
    if (ok && minDistRamp < 0.4f) { err = "physicsmeshtest: la caja atraveso la rampa malla"; ok = false; }
    if (ok && fabsf(cRamp->pos.z) < 0.5f) { err = "physicsmeshtest: la caja no se deslizo por la rampa"; ok = false; }
    // 3) la pared en x=60: el centro no pasa de 59.5
    if (ok && cWall->pos.x > 59.55f) { err = "physicsmeshtest: la caja cruzo la pared malla"; ok = false; }
    // 3b) el escalon de 20 cm (de x=90 a 94): encima de el, los pies a ~0.2; el de 60 cm: frenado antes de x=90
    printf("      [fisicamallatest] escalon 20cm: y encima=%.2f (x final %.2f) | muro 60cm: x=%.2f y=%.2f\n",
           yStep, pj1->pos.x, pj2->pos.x, pj2->pos.y);
    if (ok && (yStep < 0.17f || yStep > 0.3f)) {
        err = "physicsmeshtest: el personaje no subio el escalon de 20 cm"; ok = false;
    }
    if (ok && pj2->pos.x > 90.7f) { err = "physicsmeshtest: el personaje atraveso el muro de 60 cm"; ok = false; }
    // 4) el rayo: de (0,5,0) hacia abajo toca la caja apoyada (a ~3.5) y, ignorandola, el piso malla a 5
    if (ok) {
        Vector3 n; Object* t = NULL;
        const float d = W3dRigidRay(Vector3(0, 5, 0), Vector3(0, -1, 0), 20.0f, cFloor, &n, &t);
        printf("      [fisicamallatest] rayo: d=%.3f n=(%.2f %.2f %.2f) %s\n", d, n.x, n.y, n.z, t ? t->name.c_str() : "-");
        if (fabsf(d - 5.0f) > 0.01f || t != floor || n.y < 0.99f) { err = "physicsmeshtest: fisicaRayo no toco el piso malla"; ok = false; }
    }
    W3dRigidClear();
    W3dColClear();
    return ok;
}

static bool CmdGameHitboxMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "gamehitboxmin: uso: gamehitboxmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "juegohitboxmin: " + e2; return false; }
    if (!HbWrite(dir + "/hb_contador.lua", kLuaCounter) || !HbWrite(dir + "/hb_mover.lua", kLuaMove) ||
        !HbWrite(dir + "/hb_despertar.lua", kLuaWake) || !HbWrite(dir + "/hb_anim.lua", kLuaAnim) ||
        !HbWrite(dir + "/hb_juez.lua", kLuaJudge)) {
        err = "gamehitboxmin: no pude escribir los .lua en " + dir; return false;
    }
    // --- PUERTA: un hitbox hijo de la puerta y un objeto que la atraviesa a paso fijo ---
    Empty* door = HbEmpty("Puerta", Vector3(0, 10, 0));
    HbScript(door, "hb_contador.lua", "prefijo", "puerta");
    Hitbox* zp = new Hitbox(door, Vector3(0, 0, 0));
    zp->SetNameObj("ZonaPuerta");
    zp->size[0] = zp->size[1] = zp->size[2] = 2.0f;
    zp->filter  = "jugador";
    Empty* mobile = HbEmpty("Movil", Vector3(-5.05f, 10, 0));
    HbScript(mobile, "hb_mover.lua", "x0", "-5.05"); HbRef(mobile, "y", "10");
    Hitbox* cm = new Hitbox(mobile, Vector3(0, 0, 0));
    cm->SetNameObj("CajaMovil");
    cm->label = "jugador";
    // --- ZONA que detecta un CUERPO RIGIDO que cae y aterriza en un piso estatico ---
    Empty* floor = HbEmpty("Piso", Vector3(20, -0.5f, 0));
    floor->physics = new W3dRigidDef();
    floor->physics->type = 0; floor->physics->box[0] = 10; floor->physics->box[1] = 1; floor->physics->box[2] = 10;
    HbScript(floor, "hb_contador.lua", "prefijo", "piso");
    Empty* box = HbEmpty("Caja", Vector3(20, 4, 0));
    box->physics = new W3dRigidDef();
    HbScript(box, "hb_contador.lua", "prefijo", "caja");
    HbScript(box, "hb_despertar.lua", NULL, NULL);
    Hitbox* zone = new Hitbox(NULL, Vector3(20, 2.5f, 0));
    zone->SetNameObj("Zona");
    zone->size[0] = 3; zone->size[1] = 1; zone->size[2] = 3;
    zone->detectBodies = true;
    HbScript(zone, "hb_contador.lua", "prefijo", "zona");
    // --- ANIMACION DE ESCENA pedida por lua (animScene): el Ascensor sube de 0 a 5 ---
    Empty* asc = HbEmpty("Ascensor", Vector3(-20, 0, 0));
    HbScript(asc, "hb_anim.lua", NULL, NULL);
    InitSceneAnimations();
    const int active = ActiveSceneAnim;
    const int idx = NewScene();
    RenameActiveScene("Subir");
    SceneAnimations[idx]->startFrame = 1; SceneAnimations[idx]->endFrame = 30; SceneAnimations[idx]->fps = 30;
    { AnimationObject ao; ao.obj = asc;
      AnimProperty& py = ListProperty(ao.Properties, AnimPosition, AnimY);
      SetKeyCurve(py, 1, 0.0f); SetKeyCurve(py, 30, 5.0f);
      ao.UpdateFirstLastFrame();
      AnimationObjects.push_back(ao); }
    SetActiveScene(active);
    // --- el JUEZ ---
    Empty* judge = HbEmpty("Juez", Vector3(0, -10, 0));
    HbScript(judge, "hb_juez.lua", NULL, NULL);
    DeselectAll(); ActiveObj = NULL;
    printf("      [juegohitboxmin] juego3dmin + puerta/zona/caja/piso/ascensor/juez en '%s'\n", dir.c_str());
    return true;
}

static bool CmdGameHitboxLog(std::istringstream& ss, std::string& err) {
    std::string dir, name; ss >> dir >> name;
    if (dir.empty() || name.empty()) { err = "gamehitboxlog: uso: gamehitboxlog <carpeta> <nombre>"; return false; }
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "gamehitboxlog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    // el juez pide quit() al terminar; el timeout es la red de seguridad
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "gamehitboxlog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string line, verdict;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[juegohitbox]");
        if (p) { verdict = p; }
    }
    fclose(f);
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [juegohitboxlog] salida=%d | %s\n", r, verdict.empty() ? "(sin veredicto del juez)" : verdict.c_str());
    if (verdict.empty()) { err = "gamehitboxlog: el juez no escribio su veredicto (los scripts no corrieron?)"; return false; }
    // el juez ya exige los valores exactos; ademas se leen de su linea (un juez viejo o roto que
    // diga OK con otros numeros no pasa)
    if (verdict.find("[juegohitbox] OK") == std::string::npos ||
        verdict.find("puerta=1/7/1") == std::string::npos || verdict.find("tocar=1/1") == std::string::npos) {
        err = "gamehitboxlog: en el juego compilado los eventos/rigidos/animacion NO dan lo mismo que en el Play";
        return false;
    }
    return true;
}

// ============================================================================
//  el despachador
// ============================================================================
bool W3dTestsHitboxCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "hitboxnew")    return CmdNewHitbox(ss, err);
    if (cmd == "hitboxinfo")     return CmdHitboxInfo(ss, err);
    if (cmd == "hitboxstats")    return CmdHitboxStats(ss, err);
    if (cmd == "hitboxadd")      return CmdHitboxAdd(ss, err);
    if (cmd == "hitboxdup")      return CmdHitboxDup(ss, err);
    if (cmd == "hitboxprops")    return CmdHitboxProps(ss, err);
    if (cmd == "sharednum")  return CmdSharedNum(ss, err);
    if (cmd == "hitboxgrid")   return CmdHitboxGrid(ss, err);
    if (cmd == "hitboxsat")      return CmdHitboxSat(ss, err);
    if (cmd == "hitboxview")    return CmdHitboxView(ss, err);
    if (cmd == "childorder")      return CmdOrderChild(ss, err);
    if (cmd == "posobject")      return CmdPosObject(ss, err);
    if (cmd == "nameactive")   return CmdNameActive(ss, err);
    if (cmd == "hitboxstep")     return CmdHitboxStep(ss, err);
    if (cmd == "hitboxclear")  { W3dHitboxClear(); return true; }
    if (cmd == "gamehitboxmin") return CmdGameHitboxMin(ss, err);
    if (cmd == "gamehitboxlog") return CmdGameHitboxLog(ss, err);
    if (cmd == "physicsmeshtest") return CmdPhysicsMeshTest(ss, err);
    handled = false;
    return false;
}
