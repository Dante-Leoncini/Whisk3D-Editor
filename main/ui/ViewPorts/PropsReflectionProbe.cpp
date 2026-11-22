// ============================================================================
//  PropsReflectionProbe.cpp — la tarjeta "Reflection Probe" del panel de propiedades (pestania 2 de la sonda de reflejo,
//  objects/ReflectionProbe.h). Como PropsSondaLuz: el panel la ARMA (PropsReflectionProbeBuild) y le pide el bindeo por
//  frame (PropsReflectionProbeUpdate).
//
//    Tipo (Horneada / Personalizada / Tiempo real), Importancia, Intensidad, Proyeccion en caja, el tamanio de la caja y el
//    origen de la captura; Resolucion, Cerca; la imagen (la elegida, en una personalizada). La HORNEADA (trazado de
//    rayos): Muestras, Rebotes y Hornear. La de TIEMPO REAL (la dibuja la GPU): cada cuanto se vuelve a dibujar. El fondo
//    es siempre el de la escena
// ============================================================================
#include "ViewPorts/Properties.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "objects/ReflectionProbe.h"
#include "objects/Textures.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "render/RayTracer.h"   // RTSonda*: hornear de a pedazos (la barra)
#include <stdio.h>
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

extern void Notify(const std::string& text, bool error);
extern void PropsLayoutDirty();
void PropsOpenMenuUnderButton(PopupMenu* menu, Button* button);   // (Properties.cpp: el estandar de los desplegables)

static ReflectionProbe* SrActive() {
    return (ActiveObj && ActiveObj->getType() == ObjectType::sondareflejo) ? (ReflectionProbe*)ActiveObj : NULL;
}
// Z-ARRIBA, como la tarjeta Transformar: el campo "Y" es la z interna y el "Z" la y
static const int kAxis[3] = { 0, 2, 1 };
static float gImportance = 1, gRes = 256, gSamples = 32, gBounces = 3;

static void Change() { g_redraw = true; }
static void ActionImportance() { ReflectionProbe* s = SrActive(); if (s) s->importance = (int)(gImportance + (gImportance >= 0 ? 0.5f : -0.5f)); }
static void ActionSamples() { ReflectionProbe* s = SrActive(); if (s) s->samples = gSamples < 1.0f ? 1 : (int)(gSamples + 0.5f); }
static void ActionBounces() { ReflectionProbe* s = SrActive(); if (s) s->bounces = gBounces < 0.0f ? 0 : (int)(gBounces + 0.5f); }

// ---- el TIPO ----
static PopupMenu* gMenuType = NULL;
static const char* kTypes[3] = { "Baked", "Custom", "Realtime" };
static void ActionPickedType(int id) {
    ReflectionProbe* s = SrActive();
    if (!s || id < 0 || id > 2) return;
    s->type = id;
    PropsLayoutDirty(); g_redraw = true;
}
static void ActionMenuType() {
    ReflectionProbe* s = SrActive();
    if (!s || !PropsActive || !PropsActive->propSrType) return;
    if (!gMenuType) { gMenuType = new PopupMenu(); gMenuType->action = ActionPickedType; }
    gMenuType->Clear();
    gMenuType->title = T("Type");
    for (int i = 0; i < 3; i++) { MenuItem* it = gMenuType->Add(T(kTypes[i]), i); if (it) it->green = (s->type == i); }
    PropsOpenMenuUnderButton(gMenuType, PropsActive->propSrType->button);
}
// ---- la RESOLUCION (el ancho del 360) ----
static PopupMenu* gMenuRes = NULL;
static const int kRes[5] = { 64, 128, 256, 512, 1024 };
static void ActionPickedRes(int id) { ReflectionProbe* s = SrActive(); if (s && id >= 0 && id < 5) s->resolution = kRes[id]; g_redraw = true; }
static void ActionMenuRes() {
    ReflectionProbe* s = SrActive();
    if (!s || !PropsActive || !PropsActive->propSrRes) return;
    if (!gMenuRes) { gMenuRes = new PopupMenu(); gMenuRes->action = ActionPickedRes; }
    gMenuRes->Clear();
    gMenuRes->title = T("Resolution");
    for (int i = 0; i < 5; i++) {
        char b[24]; snprintf(b, sizeof(b), "%d x %d", kRes[i], kRes[i] / 2);
        MenuItem* it = gMenuRes->Add(b, i); if (it) it->green = (s->resolution == kRes[i]);
    }
    PropsOpenMenuUnderButton(gMenuRes, PropsActive->propSrRes->button);
}
// ---- la IMAGEN de una personalizada: una textura del proyecto (una 360) ----
static PopupMenu* gMenuImg = NULL;
static void ActionPickedImg(int id) {
    ReflectionProbe* s = SrActive();
    if (!s) return;
    if (id == 0) s->texture.clear();
    else { const size_t i = (size_t)(id - 1) + 5; if (i < Textures.size() && Textures[i]) s->texture = Textures[i]->path; }
    s->Image();
    PropsLayoutDirty(); g_redraw = true;
}
static void ActionMenuImg() {
    ReflectionProbe* s = SrActive();
    if (!s || !PropsActive || !PropsActive->propSrImage) return;
    if (!gMenuImg) { gMenuImg = new PopupMenu(); gMenuImg->action = ActionPickedImg; }
    gMenuImg->Clear();
    gMenuImg->title = T("360 Image");
    gMenuImg->Add(T("None"), 0);
    for (size_t i = 5; i < Textures.size(); i++) {
        if (!Textures[i]) continue;
        std::string n = Textures[i]->path;
        const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
        MenuItem* it = gMenuImg->Add(n, 1 + (int)(i - 5), IconType::texture);
        if (it) it->green = (Textures[i]->path == s->texture);
    }
    PropsOpenMenuUnderButton(gMenuImg, PropsActive->propSrImage->button);
}
// HORNEAR DE A PEDAZOS: la barra del progreso y Cancelar, como el horneado de la luz (PropsBake.cpp). El trabajo
// avanza en PropsReflectionProbeUpdate, una vez por cuadro
static ReflectionProbe* gSrBaking = NULL;
static float gSrProgress = 0.0f;
static unsigned int gSrT0 = 0;
extern unsigned int w3dGetTicks();
static bool SrLive(const ReflectionProbe* s) {
    std::vector<ReflectionProbe*> v; ReflectionProbeList(v);
    for (size_t i = 0; i < v.size(); i++) if (v[i] == s) return true;
    return false;
}
static void ActionCancel() {
    if (RTProbeType() != 2) return;
    RTProbeCancel(); gSrBaking = NULL;
    Notify(T("Bake canceled"), true); PropsLayoutDirty(); g_redraw = true;
}
static void ActionBake() {
    if (RTProbeType() == 2) { ActionCancel(); return; }
    ReflectionProbe* s = SrActive();
    if (!s) return;
    if (RTProbeActive()) { Notify(T("Another probe is baking"), true); return; }
    std::string reason;
    gSrT0 = w3dGetTicks();
    if (!ReflectionProbeBakeBegin(s, &reason)) { Notify(std::string(T("Not baked")) + ": " + T(reason.c_str()), true); return; }
    gSrBaking = s; gSrProgress = 0.0f;
    PropsLayoutDirty();
    g_redraw = true;
}
static void Finish() {
    ReflectionProbe* s = gSrBaking; gSrBaking = NULL;
    std::string reason;
    if (!s || !SrLive(s)) { RTProbeCancel(); Notify(T("Not baked"), true); }
    else if (!ReflectionProbeBakeFinish(s, &reason)) Notify(std::string(T("Not baked")) + ": " + T(reason.c_str()), true);
    else { char b[96]; snprintf(b, sizeof(b), T("Reflection probe baked (%u ms)"), w3dGetTicks() - gSrT0); Notify(b, false); }
    PropsLayoutDirty();
    g_redraw = true;
}

static PropFloat* Row(const char* name, const char* unit, float mn, float mx, float fine, float thick) {
    PropFloat* f = new PropFloat(T(name), unit);
    f->SetRange(mn, mx);
    f->fineStep = fine; f->thickStep = thick; f->dragStep = fine * 0.1f;
    f->onChange = Change;
    return f;
}
static PropButton* Dropdown(const char* name, void (*action)()) {
    PropButton* b = new PropButton(T(name));
    b->withLabel = true; b->inColumn = true;
    b->button->dropdown = true;
    b->action = action;
    return b;
}

void PropsReflectionProbeBuild(Properties* p) {
    if (!p) return;
    p->propProbeRef = new GroupPropertie(T("Reflection Probe"));
    p->propSrType = Dropdown("Type", ActionMenuType);
    p->propProbeRef->properties.push_back(p->propSrType);
    p->propSrImportance = Row("Importance", "", -100.0f, 100.0f, 1.0f, 10.0f);
    p->propSrImportance->integer = true;
    p->propSrImportance->onChange = ActionImportance;
    p->propProbeRef->properties.push_back(p->propSrImportance);
    p->propSrIntensity = Row("Intensity", "", 0.0f, 16.0f, 0.1f, 1.0f);
    p->propProbeRef->properties.push_back(p->propSrIntensity);
    p->propSrBox = new PropBool(T("Box Projection"));
    p->propSrBox->onChange = Change;
    p->propProbeRef->properties.push_back(p->propSrBox);
    const char* sizes[3] = { "Size X", "Size Y", "Size Z" };
    for (int k = 0; k < 3; k++) { p->propSrSize[k] = Row(sizes[k], "m", 0.01f, 100000.0f, 0.1f, 1.0f); p->propProbeRef->properties.push_back(p->propSrSize[k]); }
    const char* ors[3] = { "Probe Origin X", "Probe Origin Y", "Probe Origin Z" };
    for (int k = 0; k < 3; k++) { p->propSrOrigin[k] = Row(ors[k], "m", -100000.0f, 100000.0f, 0.1f, 1.0f); p->propProbeRef->properties.push_back(p->propSrOrigin[k]); }
    p->propSrRes = Dropdown("Resolution", ActionMenuRes);
    p->propProbeRef->properties.push_back(p->propSrRes);
    p->propSrNear = Row("Near", "m", 0.0f, 1000.0f, 0.05f, 0.5f);
    p->propProbeRef->properties.push_back(p->propSrNear);
    p->propSrSamples = Row("Samples", "", 1.0f, 1024.0f, 1.0f, 8.0f);
    p->propSrSamples->integer = true;
    p->propSrSamples->onChange = ActionSamples;
    p->propProbeRef->properties.push_back(p->propSrSamples);
    p->propSrBounces = Row("Bounces", "", 0.0f, 8.0f, 1.0f, 1.0f);
    p->propSrBounces->integer = true;
    p->propSrBounces->onChange = ActionBounces;
    p->propProbeRef->properties.push_back(p->propSrBounces);
    p->propSrEach = Row("Update every", "s", 0.0f, 600.0f, 0.1f, 1.0f);
    p->propProbeRef->properties.push_back(p->propSrEach);
    p->propSrImage = Dropdown("360 Image", ActionMenuImg);
    p->propProbeRef->properties.push_back(p->propSrImage);
    p->propSrBake = new PropButton(T("Bake"), IconType::photo);
    p->propSrBake->action = ActionBake;
    p->propProbeRef->properties.push_back(p->propSrBake);
    p->propSrBar = new PropButton("0%");   // (la barra del progreso, solo mientras hornea)
    p->propSrBar->button->bar = 0.0f;
    p->propSrBar->hidden = true;
    p->propProbeRef->properties.push_back(p->propSrBar);
    p->propSrCancel = new PropButton(T("Cancel"), IconType::close);
    p->propSrCancel->action = ActionCancel;
    p->propSrCancel->hidden = true;
    p->propProbeRef->properties.push_back(p->propSrCancel);
    p->propSrState = new PropLabel(T("Not baked"), true);
    p->propProbeRef->properties.push_back(p->propSrState);
    p->GroupProperties.push_back(p->propProbeRef);
}

static void BindF(PropFloat* f, float* v) { if (f && f->value != v) { f->value = v; PropsLayoutDirty(); } }
static void BindB(PropBool* b, bool* v) { if (b && b->value != v) { b->value = v; PropsLayoutDirty(); } }
static void Hide(PropButton* b, bool o) { if (b && b->hidden != o) { b->hidden = o; PropsLayoutDirty(); } }

void PropsReflectionProbeUpdate(Properties* p, bool visible) {
    if (!p || !p->propProbeRef) return;
    // EL TRABAJO avanza una vez por cuadro (aunque haya varios paneles): ~25 ms por cuadro, el editor sigue andando
    if (RTProbeType() == 2 && gSrBaking) {
        static unsigned int last = 0;
        if (w3dGetTicks() - last >= 10) {
            last = w3dGetTicks();
            gSrProgress = RTProbeStep(25);
            if (!RTProbeActive()) Finish();
            g_redraw = true;
        }
    }
    ReflectionProbe* s = visible ? SrActive() : NULL;
    p->propProbeRef->visible = (s != NULL);
    if (!s) {
        BindF(p->propSrImportance, NULL); BindF(p->propSrIntensity, NULL); BindB(p->propSrBox, NULL);
        for (int k = 0; k < 3; k++) { BindF(p->propSrSize[k], NULL); BindF(p->propSrOrigin[k], NULL); }
        BindF(p->propSrNear, NULL); BindF(p->propSrEach, NULL);
        BindF(p->propSrSamples, NULL); BindF(p->propSrBounces, NULL);
        return;
    }
    const bool horneable = s->type != W3D_REFLECTION_CUSTOMIZED;
    if (p->propSrType) { const std::string t = T(kTypes[s->type < 0 || s->type > 2 ? 0 : s->type]);
                         if (p->propSrType->button->text != t) { p->propSrType->button->text = t; g_redraw = true; } }
    if (g_propFloatEditing != p->propSrImportance) gImportance = (float)s->importance;
    BindF(p->propSrImportance, &gImportance);
    BindF(p->propSrIntensity, horneable ? &s->intensity : NULL);
    BindB(p->propSrBox, &s->box);
    for (int k = 0; k < 3; k++) { BindF(p->propSrSize[k], &s->size[kAxis[k]]); BindF(p->propSrOrigin[k], &s->origin[kAxis[k]]); }
    Hide(p->propSrRes, !horneable);
    if (p->propSrRes) { char b[24]; snprintf(b, sizeof(b), "%d x %d", s->resolution, s->resolution / 2);
                        if (p->propSrRes->button->text != b) { p->propSrRes->button->text = b; g_redraw = true; } }
    BindF(p->propSrNear, horneable ? &s->near : NULL);
    BindF(p->propSrEach, s->type == W3D_REFLECTION_TIME_REAL ? &s->eachSeg : NULL);
    // (las del trazado de rayos: solo la horneada)
    const bool baked = s->type == W3D_REFLECTION_BAKED;
    if (g_propFloatEditing != p->propSrSamples) gSamples = (float)s->samples;
    if (g_propFloatEditing != p->propSrBounces) gBounces = (float)s->bounces;
    BindF(p->propSrSamples, baked ? &gSamples : NULL);
    BindF(p->propSrBounces, baked ? &gBounces : NULL);
    Hide(p->propSrImage, horneable);
    if (p->propSrImage) {
        std::string n = s->texture.empty() ? std::string(T("None")) : s->texture;
        const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
        if (p->propSrImage->button->text != n) { p->propSrImage->button->text = n; g_redraw = true; }
    }
    // HORNEAR y su estado: solo la horneada (la de tiempo real la dibuja la GPU; la personalizada usa su imagen)
    const bool live = s->type == W3D_REFLECTION_TIME_REAL;
    // MIENTRAS HORNEA (esta sonda): sin el boton, la barra y Cancelar
    const bool working = s == gSrBaking && RTProbeType() == 2;
    Hide(p->propSrBake, !horneable || live || working);
    Hide(p->propSrBar, !working);
    Hide(p->propSrCancel, !working);
    if (working && p->propSrBar) {
        char b[32]; snprintf(b, sizeof(b), "%d%%", (int)(gSrProgress * 100.0f));
        p->propSrBar->button->text = b;
        p->propSrBar->button->bar = gSrProgress;
    }
    if (p->propSrState) {
        const bool oc = live || s->type == W3D_REFLECTION_CUSTOMIZED;
        if (p->propSrState->hidden != oc) { p->propSrState->hidden = oc; PropsLayoutDirty(); }
        std::string t;
        if (s->texture.empty()) t = T("Not baked");
        else { std::string n = s->texture; const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
               t = std::string(T("Baked")) + ": " + n; }
        if (p->propSrState->name != t) { p->propSrState->name = t; PropsLayoutDirty(); }
    }
}
