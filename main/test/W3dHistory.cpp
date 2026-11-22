// La GRABADORA DE PASOS (ver W3dHistory.h): W3D_HISTORY=<ruta.txt> anota lo que la persona hace en el editor.
#ifdef W3D_MCP

#include "test/W3dHistory.h"
#include "app/variables.h"            // cursor3D, transformOrientation, EditSelectMode
#include "ViewPorts/ViewPorts.h"      // ViewportBase, FindViewportUnderMouse, BarButtons / ToolButtons
#include "ViewPorts/ViewPort3D.h"     // Viewport3D
#include "ViewPorts/Gizmo.h"          // GizmoHandleInScreen, GizmoMode
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/PopUp/PopUpBase.h" // PopUpActive
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/Tab.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/draw/icons.h"      // IconName
#include "WhiskUI/core/UI.h"         // GlobalScale, RowHeightGS...
#include "render/RenderOptions.h"    // g_transformPivot
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/EditMesh.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#ifdef __GNUC__
#include <cxxabi.h>
#endif

extern ViewportBase* rootViewport;
extern int OutlinerObjectRow(Object* o);
extern int OutlinerYOfRow(int row);   // (con los cortes constructor / cada cuadro)
extern Object* g_editMesh;

static FILE* g_f = NULL;
static Uint32 g_t0 = 0;
static int g_pend = 0;                       // cuadros que faltan para mirar lo que cambio
static std::map<std::string, std::string> g_prev;   // lo ultimo anotado de cada cosa
static std::string g_keys;                 // teclas seguidas, en una sola linea
static Uint32 g_keysT = 0;
static int g_downX = 0, g_downY = 0, g_downButton = 0;
static bool g_down = false;
static int g_wheel = 0;

bool W3dHistoryActive() { return g_f != NULL; }

static void Line(const char* fmt, ...) {
    if (!g_f) return;
    const Uint32 t = SDL_GetTicks() - g_t0;
    fprintf(g_f, "[%02u:%02u.%u] ", t / 60000, (t / 1000) % 60, (t / 100) % 10);
    va_list ap; va_start(ap, fmt); vfprintf(g_f, fmt, ap); va_end(ap);
    fputc('\n', g_f);
    fflush(g_f);
}

static void ReleaseKeys() {
    if (g_keys.empty()) return;
    Line("teclas: %s", g_keys.c_str());
    g_keys.clear();
}
static void ReleaseWheel() {
    if (!g_wheel) return;
    Line("wheel: %+d (zoom)", g_wheel);
    g_wheel = 0;
}

void W3dHistoryStart() {
    const char* path = getenv("W3D_HISTORY");
    if (!path || !*path) return;
    g_f = fopen(path, "a");
    if (!g_f) { fprintf(stderr, "W3D_HISTORY: no se pudo abrir '%s'\n", path); return; }
    g_t0 = SDL_GetTicks();
    fprintf(g_f, "\n==== grabadora de pasos de Whisk3D ====\n"
                 "(posiciones y medidas como en Propiedades: X, Y, Z con Z arriba, en metros)\n");
    fflush(g_f);
    g_pend = 1;
}

// ---------------------------------------------------------------------------------------------------------------
//  Que hay debajo de un click
// ---------------------------------------------------------------------------------------------------------------
static const char* ViewName(int kind) {
    switch (kind) {
        case 1: return "Vista 3D";
        case 2: return "Outliner";
        case 3: return "Propiedades";
        case 4: return "Editor UV";
        case 5: return "Linea de tiempo";
        case 6: return "Editor 2D";
        case 7: return "Consola";
        case 8: return "Editor de scripts";
        default: return "viewport";
    }
}

static bool Inside(int x, int y, int bx, int by, int bw, int bh) { return x >= bx && x < bx + bw && y >= by && y < by + bh; }

// el nombre de cada boton de la Vista 3D (en la PC la barra de menus es casi toda de iconos): ver BR_* / TBR_* en ViewPort3D.h
static const char* RoleBar3D(int role) {
    switch (role) {
        case 1: return "Modo (Objeto / Edicion...)"; case 2: return "Vertex / Arista / Cara"; case 3: return "Punto de pivote";
        case 4: return "Seleccionar"; case 5: return "Aniadir (+)"; case 6: return "Objeto"; case 7: return "Superposiciones";
        case 8: return "Render (sombreado)"; case 9: return "Orientacion"; case 10: return "UV"; case 11: return "Vista";
        case 12: return "Imantado"; case 15: return "Malla"; case 16: return "Animacion"; case 17: return "Edicion proporcional";
        case 18: return "Escena / prefab"; case 19: return "Guardar textura";
        default: return NULL;
    }
}
static const char* RoleTools3D(int role) {
    switch (role) {
        case 100: return "la tilde verde (Aceptar)"; case 101: return "la cruz (Cancelar)"; case 102: return "Orientacion";
        case 103: return "X"; case 104: return "Y"; case 105: return "Z"; case 106: return "Shift"; case 107: return "Ctrl";
        case 108: return "Deshacer"; case 109: return "Rehacer"; case 120: return "Repetir"; case 121: return "Vista";
        case 122: return "Edicion proporcional";
        default: return NULL;
    }
}
static std::string ButtonText(Button* b, int kind, bool tools) {
    char t[256];
    const char* ic = (b->icon >= 0) ? IconName(b->icon) : NULL;
    const char* role = (kind != 1) ? NULL : tools ? RoleTools3D(b->role) : RoleBar3D(b->role);
    if (!b->text.empty()) snprintf(t, sizeof t, "'%s'%s%s", b->text.c_str(), role ? " = " : "", role ? role : "");
    else if (role) snprintf(t, sizeof t, "%s (el icono %s)", role, ic ? ic : "?");
    else snprintf(t, sizeof t, "el icono %s (rol %d)", ic ? ic : "?", b->role);
    return t;
}

static std::string RowProperties(Properties* P, int mx, int my) {
    for (size_t i = 0; i < P->BarTabs.size(); i++) {
        Tab* t = P->BarTabs[i];
        if (t->visible && Inside(mx, my, t->sx, t->sy, t->width, t->height)) {
            char b[64]; snprintf(b, sizeof b, "pestania %d", (int)i); return b;
        }
    }
    // el mismo recorrido que Properties::ClickIn (y que capinfo)
    int yCursor = P->y + P->BarTopOffset() + P->PosY + borderGS + RowHeightGS + gapGS;
    for (size_t i = 0; i < P->GroupProperties.size(); i++) {
        GroupPropertie* g = P->GroupProperties[i];
        if (!g->visible) continue;
        const int hCab = borderGS + RowHeightGS + gapGS;
        if (my >= yCursor && my < yCursor + hCab) return "tarjeta '" + g->name + "' (el titulo)";
        if (g->open) {
            int yRow = yCursor + hCab;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                const int hRow = prop->Resize(g->width);
                if (hRow > 0 && my >= yRow && my < yRow + hRow)
                    return "tarjeta '" + g->name + "', fila '" + prop->name + "'";
                yRow += hRow;
            }
        }
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    return "";
}

static std::string RowOutliner(Outliner* o, Object* root, int my) {
    if (!root) return "";
    for (size_t i = 0; i < root->Childrens.size(); i++) {
        Object* ob = root->Childrens[i];
        const int row = OutlinerObjectRow(ob);
        if (row >= 0) {
            const int y = o->y + borderGS + o->PosY + o->BarTopOffset() + OutlinerYOfRow(row);
            if (my >= y && my < y + (int)RowHeightGS) return "fila '" + ob->name + "'";
        }
        std::string s = RowOutliner(o, ob, my);
        if (!s.empty()) return s;
    }
    return "";
}

static std::string ThatHas(int mx, int my) {
    if (OpenMenu && OpenMenu->open && OpenMenu->ContainsWithSubmenus(mx, my)) return "menu abierto";
    if (PopUpActive) return "window emergente";
    if (!rootViewport) return "";
    ViewportBase* v = FindViewportUnderMouse(rootViewport, mx, my);
    if (!v) return "";
    std::string s = ViewName(v->ViewportKind());
    for (size_t i = 0; i < v->BarButtons.size(); i++) {
        Button* b = v->BarButtons[i];
        if (b->visible && Inside(mx, my, b->sx, b->sy, b->width, b->height)) return s + ", barra de menus: " + ButtonText(b, v->ViewportKind(), false);
    }
    if (v->ToolbarVisible()) {
        for (size_t i = 0; i < v->ToolButtons.size(); i++) {
            Button* b = v->ToolButtons[i];
            if (b->visible && Inside(mx, my, b->sx, b->sy, b->width, b->height)) return s + ", barra de herramientas: " + ButtonText(b, v->ViewportKind(), true);
        }
    }
    if (v->ViewportKind() == 3) { std::string f = RowProperties((Properties*)v, mx, my); if (!f.empty()) return s + ", " + f; }
    if (v->ViewportKind() == 2 && ((Outliner*)v)->view == OUT_VIEW_SCENE) {
        std::string f = RowOutliner((Outliner*)v, SceneCollection, my); if (!f.empty()) return s + ", " + f;
    }
    if (v->ViewportKind() == 1) {
        static const char* kMan[] = { "flecha X (roja)", "flecha Y (verde)", "flecha Z (azul)", "plano X", "plano Y", "plano Z",
                                      "center", "anillo de la vista", "interior (giro libre)" };
        static const char* kMode[] = { "move", "rotate", "scale" };
        int best = -1; float dBest = 14.0f * GlobalScale;
        for (int k = GizmoAxisX; k <= GizmoFree; k++) {
            float sx = 0, sy = 0;
            if (!GizmoHandleInScreen((Viewport3D*)v, k, sx, sy)) continue;
            const float d = hypotf(v->x + sx - mx, v->y + sy - my);
            if (d < dBest) { dBest = d; best = k; }
        }
        if (best >= 0) {
            const int m = GizmoMode();
            return s + ", gizmo (" + (m >= 0 && m <= 2 ? kMode[m] : "?") + "): " + kMan[best];
        }
        char b[64]; snprintf(b, sizeof b, " (%d, %d dentro de la vista)", mx - v->x, my - v->y);
        return s + b;
    }
    return s;
}

// ---------------------------------------------------------------------------------------------------------------
//  Los eventos
// ---------------------------------------------------------------------------------------------------------------
static void Pending();
static const char* ButtonName(int b) { return b == 1 ? "left" : b == 2 ? "del medio" : b == 3 ? "right" : "?"; }

void W3dHistoryEvent(const SDL_Event& e) {
    if (!g_f) return;
    switch (e.type) {
        case SDL_KEYDOWN: {
            if (e.key.repeat) return;
            const SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL || k == SDLK_RCTRL || k == SDLK_LALT || k == SDLK_RALT) return;
            ReleaseWheel();
            std::string t;
            const Uint16 mod = e.key.keysym.mod;
            if (mod & KMOD_CTRL) t += "Ctrl+";
            if (mod & KMOD_SHIFT) t += "Shift+";
            if (mod & KMOD_ALT) t += "Alt+";
            t += SDL_GetKeyName(k);
            Pending();
            if (!g_keys.empty() && SDL_GetTicks() - g_keysT > 2500) ReleaseKeys();   // (una pausa larga: otra linea)
            if (!g_keys.empty()) g_keys += ", ";
            g_keys += t;
            g_keysT = SDL_GetTicks();
            g_pend = 1;
            return;
        }
        case SDL_MOUSEBUTTONDOWN: {
            Pending();
            ReleaseKeys(); ReleaseWheel();
            const bool finger = (e.button.which == SDL_TOUCH_MOUSEID);
            g_down = true; g_downX = e.button.x; g_downY = e.button.y; g_downButton = e.button.button;
            if (OpenMenu && OpenMenu->open && OpenMenu->ContainsWithSubmenus(e.button.x, e.button.y)) { g_pend = 1; return; }   // (la opcion la anota PickedMenu)
            Line("%s %s%s en (%d, %d): %s", finger ? "tap" : "click", finger ? "" : ButtonName(e.button.button),
                  e.button.clicks > 1 ? " (doble)" : "", e.button.x, e.button.y, ThatHas(e.button.x, e.button.y).c_str());
            g_pend = 1;
            return;
        }
        case SDL_MOUSEBUTTONUP: {
            if (!g_down) return;
            g_down = false;
            const int d = abs(e.button.x - g_downX) + abs(e.button.y - g_downY);
            if (d > 6 * GlobalScale)
                Line("  arrastre (boton %s) hasta (%d, %d): %d px a la derecha, %d px hacia abajo", ButtonName(g_downButton),
                      e.button.x, e.button.y, e.button.x - g_downX, e.button.y - g_downY);
            g_pend = 1;
            return;
        }
        case SDL_MOUSEWHEEL:
            ReleaseKeys();
            g_wheel += e.wheel.y;
            g_pend = 1;
            return;
        default: return;
    }
}

// la opcion elegida en un menu, con su ruta (el hook de WhiskUI)
static void PickedMenu(const PopupMenu* m, const MenuItem* it) {
    if (!g_f || !it) return;
    ReleaseKeys(); ReleaseWheel();
    std::string path = it->text;
    for (const PopupMenu* p = m->parent; p; p = p->parent) {
        if (p->selectIndex >= 0 && p->selectIndex < (int)p->items.size()) path = p->items[p->selectIndex]->text + " > " + path;
        else path = "? > " + path;
    }
    if (!m->parent && !m->title.empty()) path = "[" + m->title + "] " + path;
    if (it->checkbox) Line("menu: %s -> %s", path.c_str(), *it->checkbox ? "on" : "off");
    else Line("menu: %s", path.c_str());
    g_pend = 1;
}

void W3dHistoryUndo(const char* type) {
    if (!g_f || !type) return;
    std::string n = type;
#ifdef __GNUC__
    int st = 0; char* d = abi::__cxa_demangle(type, NULL, NULL, &st);
    if (d && st == 0) n = d;
    free(d);
#endif
    ReleaseKeys();
    Line("  paso del Ctrl+Z: %s", n.c_str());
}

// ---------------------------------------------------------------------------------------------------------------
//  Lo que cambio (despues de cada cosa)
// ---------------------------------------------------------------------------------------------------------------
static std::string F3(float x, float y, float z) {      // interno (Y arriba) -> como Propiedades (Z arriba)
    char b[96]; snprintf(b, sizeof b, "X %.4g, Y %.4g, Z %.4g", fabsf(x) < 5e-5f ? 0.0f : x, fabsf(z) < 5e-5f ? 0.0f : z, fabsf(y) < 5e-5f ? 0.0f : y);
    return b;
}
static std::string Num(int n) { char b[16]; snprintf(b, sizeof b, "%d", n); return b; }

static const char* TypeName(Object* o) {
    switch ((int)o->getType()) {
        case ObjectType::mesh: return "mesh";
        case ObjectType::camera: return "camera";
        case ObjectType::light: return "light";
        case ObjectType::empty: return "empty";
        case ObjectType::armature: return "armature";
        case ObjectType::curve: return "curve";
        case ObjectType::collection: return "collection";
        default: return "object";
    }
}

static void Objects(Object* root, std::string& s) {
    if (!root) return;
    for (size_t i = 0; i < root->Childrens.size(); i++) {
        Object* o = root->Childrens[i];
        if (!s.empty()) s += ", ";
        s += o->name + " (" + TypeName(o) + ")";
        if (o->Childrens.size()) { std::string h; Objects(o, h); s += " [" + h + "]"; }
    }
}

static void State(std::map<std::string, std::string>& e) {
    static const char* kModes[] = { "Objeto", "Edicion", "Pintura de vertices", "Pintura de pesos", "Pintura de textura", "Pose" };
    std::string mode = (InteractionMode >= 0 && InteractionMode <= 5) ? kModes[InteractionMode] : "?";
    if (InteractionMode == EditMode) mode += EditSelectMode == SelVertex ? " (vertices)" : EditSelectMode == SelEdge ? " (aristas)" : " (caras)";
    e["mode"] = mode;
    { std::string s; Objects(SceneCollection, s); e["scene"] = s; }
    { std::string s; for (size_t i = 0; i < ObjSelects.size(); i++) { if (i) s += ", "; s += ObjSelects[i]->name; } e["picked"] = s.empty() ? "(nada)" : s; }
    e["cursor 3D"] = F3(cursor3D.pos.x, cursor3D.pos.y, cursor3D.pos.z);
    static const char* kPiv[] = { "cursor 3D", "origenes individuales", "punto medio", "elemento activo" };
    e["pivot"] = (g_transformPivot >= 0 && g_transformPivot <= 3) ? kPiv[g_transformPivot] : "?";
    static const char* kOri[] = { "Global", "Local", "Vista", "Normal" };
    e["orientation"] = (transformOrientation >= 0 && transformOrientation <= 3) ? kOri[transformOrientation] : "?";
    { static const char* kModeG[] = { "move", "rotate", "scale" }; const int g = GizmoMode(); e["gizmo"] = (g >= 0 && g <= 2) ? kModeG[g] : "?"; }

    Object* o = ActiveObj;
    if (!o) { e["active"] = "(ninguno)"; return; }
    e["active"] = o->name + " (" + TypeName(o) + ")";
    o->UpdateDisplayRot();
    {
        char b[256];
        snprintf(b, sizeof b, "posicion %s | rotacion (Propiedades) %.4g, %.4g, %.4g | escala (interna x, y, z) %.4g, %.4g, %.4g",
                 F3(o->pos.x, o->pos.y, o->pos.z).c_str(), o->rotEuler.x, o->rotEuler.y, o->rotEuler.z, o->scale.x, o->scale.y, o->scale.z);
        e["transformacion de " + o->name] = b;
    }
    if (o->getType() != ObjectType::mesh) return;
    Mesh* m = (Mesh*)o;
    // las MEDIDAS (la caja de la malla, en local, por la escala) y lo elegido en el Modo Edicion
    const bool edit = (InteractionMode == EditMode && g_editMesh == o && m->edit);
    float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
    float smn[3] = { 1e30f, 1e30f, 1e30f }, smx[3] = { -1e30f, -1e30f, -1e30f };
    int nv = 0, nsel = 0;
    if (edit) {
        EditMesh* em = m->edit;
        nv = em->NumVerts();
        // lo elegido segun el modo: en Cara y en Arista, vertSel no siempre acompania (se arma desde las caras / aristas)
        std::vector<unsigned char> sel((size_t)nv, 0);
        if (EditSelectMode == SelFace) {
            for (size_t f = 0; f < em->faceSel.size() && f < em->faces.size(); f++)
                if (em->faceSel[f]) for (size_t j = 0; j < em->faces[f].size(); j++) { const int k = em->faces[f][j]; if (k >= 0 && k < nv) sel[(size_t)k] = 1; }
        } else if (EditSelectMode == SelEdge) {
            for (size_t a = 0; a < em->edgeSel.size() && a * 2 + 1 < em->lineIdx.size(); a++)
                if (em->edgeSel[a]) { const int k0 = em->lineIdx[a * 2], k1 = em->lineIdx[a * 2 + 1]; if (k0 < nv) sel[(size_t)k0] = 1; if (k1 < nv) sel[(size_t)k1] = 1; }
        } else {
            for (int k = 0; k < nv && k < (int)em->vertSel.size(); k++) sel[(size_t)k] = em->vertSel[(size_t)k];
        }
        for (int k = 0; k < nv; k++) {
            const float* p = &em->pos[(size_t)k * 3];
            for (int a = 0; a < 3; a++) { if (p[a] < mn[a]) mn[a] = p[a]; if (p[a] > mx[a]) mx[a] = p[a]; }
            if (sel[(size_t)k]) {
                nsel++;
                for (int a = 0; a < 3; a++) { if (p[a] < smn[a]) smn[a] = p[a]; if (p[a] > smx[a]) smx[a] = p[a]; }
            }
        }
        int ns = 0, fs = 0, is = 0;
        for (size_t f = 0; f < em->faceSel.size(); f++) if (em->faceSel[f]) fs++;
        for (size_t a = 0; a < em->edgeSel.size(); a++) if (em->edgeSel[a]) is++;
        ns = nsel;
        std::string s = std::string(EditSelectMode == SelFace ? Num(fs) + " faces" : EditSelectMode == SelEdge ? Num(is) + " aristas" : Num(ns) + " vertices") +
                        " elegidas, " + Num(ns) + " vertices en total (de " + Num(nv) + " vertices, " +
                        Num(em->NumEdges()) + " aristas y " + Num(em->NumFaces()) + " caras)";
        if (nsel) {
            s += " | centro " + F3((smn[0] + smx[0]) * 0.5f, (smn[1] + smx[1]) * 0.5f, (smn[2] + smx[2]) * 0.5f) +
                 " | tamanio " + F3(smx[0] - smn[0], smx[1] - smn[1], smx[2] - smn[2]) + " (en local)";
        }
        e["selection de " + o->name] = s;
    } else {
        nv = m->vertexSize;
        for (int k = 0; k < nv; k++) {
            const GLfloat* p = &m->vertex[(size_t)k * 3];
            for (int a = 0; a < 3; a++) { if (p[a] < mn[a]) mn[a] = p[a]; if (p[a] > mx[a]) mx[a] = p[a]; }
        }
        e["selection de " + o->name] = "";
    }
    if (nv) {
        e["medidas de " + o->name] = F3((mx[0] - mn[0]) * o->scale.x, (mx[1] - mn[1]) * o->scale.y, (mx[2] - mn[2]) * o->scale.z) +
                                     " | la box de la malla (local) va de " + F3(mn[0], mn[1], mn[2]) + " a " + F3(mx[0], mx[1], mx[2]);
    }
    {
        std::string s;
        for (size_t i = 0; i < m->modifiers.size(); i++) { if (i) s += ", "; s += m->ModifierName((int)i); }
        e["modificadores de " + o->name] = s.empty() ? "(ninguno)" : s;
    }
}

// en medio de una operacion (mover, rotar, el arrastre del gizmo, un boton apretado) no: se anota como quedo al terminar
static bool Still() { return state == editNavigation && !GizmoDragging() && !g_down; }

static void Snapshot() {
    g_pend = 0;
    std::map<std::string, std::string> e;
    State(e);
    bool something = false;
    for (std::map<std::string, std::string>::iterator it = e.begin(); it != e.end(); ++it) {
        std::map<std::string, std::string>::iterator a = g_prev.find(it->first);
        if (a != g_prev.end() && a->second == it->second) continue;
        if (it->second.empty() && a == g_prev.end()) continue;
        if (!something) { ReleaseKeys(); ReleaseWheel(); something = true; }
        Line("  = %s: %s", it->first.c_str(), it->second.c_str());
    }
    g_prev = e;
}

// lo pendiente de la accion anterior, ANTES de anotar la siguiente (el editor en reposo no dibuja cuadros de mas)
static void Pending() { if (g_pend > 0 && Still()) Snapshot(); }

void W3dHistoryFrame() {
    if (!g_f) return;
    // el gancho de los menus (WhiskUI no conoce al editor): se instala aca, una vez
    if (!W3dPickedMenuHook) W3dPickedMenuHook = PickedMenu;
    if (!g_keys.empty() && SDL_GetTicks() - g_keysT > 2500) ReleaseKeys();
    // este cuadro arranca con los eventos del anterior ya procesados: lo que cambio ya se ve
    if (g_pend > 0 && Still()) Snapshot();
}

#endif // W3D_MCP
