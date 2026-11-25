// ============================================================================
//  PropsLightProbe.cpp — la tarjeta "Light Probe" del panel de propiedades (pestania 2 de la sonda de luz,
//  objects/LightProbe.h). Como PropsHitbox: el panel la ARMA (PropsLightProbeBuild) y le pide el bindeo por frame
//  (PropsLightProbeUpdate).
//
//  El tamano bindea directo; los PUNTOS y los RAYOS son enteros (filas de numero con su copia en float): cambiar los
//  puntos BORRA lo horneado (la grilla ya no es la misma). "Bake" hornea con el trazado de rayos de la CPU.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/LightProbe.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "render/RayTracer.h"   // RTSonda*: hornear de a pedazos (la barra)
#include <stdio.h>

extern void Notify(const std::string& text, bool error);
extern void PropsLayoutDirty();   // (el estado cambio de largo: rearmar la tarjeta)

static LightProbe* SlActive() {
    return (ActiveObj && ActiveObj->getType() == ObjectType::lightProbe) ? (LightProbe*)ActiveObj : NULL;
}
// Z-ARRIBA, como la tarjeta Transformar: el campo "Y" es la z interna y el "Z" la y
static const int kAxis[3] = { 0, 2, 1 };
static float gPoints[3] = { 3, 3, 3 };
static float gRays = 256;

static void ActionPoints() {
    LightProbe* s = SlActive();
    if (!s) return;
    bool change = false;
    for (int k = 0; k < 3; k++) {
        int n = (int)(gPoints[k] + 0.5f); if (n < 2) n = 2; if (n > 16) n = 16;
        if (s->points[kAxis[k]] != n) { s->points[kAxis[k]] = n; change = true; }
    }
    if (change) s->sh.clear();   // (la grilla es otra: lo horneado no vale)
    g_redraw = true;
}
static void ActionRays() {
    LightProbe* s = SlActive();
    if (!s) return;
    int n = (int)(gRays + 0.5f); if (n < 16) n = 16; if (n > 8192) n = 8192;
    s->rays = n;
}
// HORNEAR DE A PEDAZOS: la barra del progreso y Cancelar, como el horneado de la luz (PropsBake.cpp). El trabajo
// avanza en PropsLightProbeUpdate, una vez por cuadro
static LightProbe* gSlBaking = NULL;
static float gSlProgress = 0.0f;
static void ActionCancel() {
    if (RTProbeType() != 1) return;
    RTProbeCancel(); gSlBaking = NULL;
    Notify(T("Bake canceled"), true); PropsLayoutDirty(); g_redraw = true;
}
static void ActionBake() {
    if (RTProbeType() == 1) { ActionCancel(); return; }
    LightProbe* s = SlActive();
    if (!s) return;
    if (RTProbeActive()) { Notify(T("Another probe is baking"), true); return; }
    std::string reason;
    if (!LightProbeBakeBegin(s, &reason)) { Notify(std::string(T("Not baked")) + ": " + T(reason.c_str()), true); return; }
    gSlBaking = s; gSlProgress = 0.0f;
    PropsLayoutDirty();
    g_redraw = true;
}
static void Finish() {
    LightProbe* s = gSlBaking; gSlBaking = NULL;
    if (!s || !LightProbeLive(s) || !LightProbeBakeFinish(s)) { RTProbeCancel(); Notify(T("Not baked"), true); }
    else { char b[96]; snprintf(b, sizeof(b), T("Light probe baked: %d points"), s->Count()); Notify(b, false); }
    PropsLayoutDirty();
    g_redraw = true;
}

void PropsLightProbeBuild(Properties* p) {
    if (!p) return;
    p->propProbe = new GroupPropertie(T("Light Probe"));
    const char* axes[3] = { "Size X", "Size Y", "Size Z" };
    for (int k = 0; k < 3; k++) {
        p->propSlSize[k] = new PropFloat(T(axes[k]), "m");
        p->propSlSize[k]->SetRange(0.01f, 100000.0f);
        p->propSlSize[k]->fineStep = 0.1f; p->propSlSize[k]->thickStep = 1.0f;
        p->propProbe->properties.push_back(p->propSlSize[k]);
    }
    const char* pts[3] = { "Points X", "Points Y", "Points Z" };
    for (int k = 0; k < 3; k++) {
        p->propSlPoints[k] = new PropFloat(T(pts[k]));
        p->propSlPoints[k]->SetRange(2.0f, 16.0f);
        p->propSlPoints[k]->integer = true;
        p->propSlPoints[k]->fineStep = 1.0f; p->propSlPoints[k]->thickStep = 1.0f; p->propSlPoints[k]->dragStep = 0.05f;
        p->propSlPoints[k]->onChange = ActionPoints;
        p->propProbe->properties.push_back(p->propSlPoints[k]);
    }
    p->propSlRays = new PropFloat(T("Rays"));
    p->propSlRays->SetRange(16.0f, 8192.0f);
    p->propSlRays->integer = true;
    p->propSlRays->fineStep = 16.0f; p->propSlRays->thickStep = 128.0f; p->propSlRays->dragStep = 2.0f;
    p->propSlRays->onChange = ActionRays;
    p->propProbe->properties.push_back(p->propSlRays);
    p->propSlBake = new PropButton(T("Bake"), IconType::sphere);
    p->propSlBake->action = ActionBake;
    p->propProbe->properties.push_back(p->propSlBake);
    p->propSlBar = new PropButton("0%");   // (la barra del progreso, solo mientras hornea)
    p->propSlBar->button->bar = 0.0f;
    p->propSlBar->hidden = true;
    p->propProbe->properties.push_back(p->propSlBar);
    p->propSlCancel = new PropButton(T("Cancel"), IconType::close);
    p->propSlCancel->action = ActionCancel;
    p->propSlCancel->hidden = true;
    p->propProbe->properties.push_back(p->propSlCancel);
    p->propSlState = new PropLabel(T("Not baked"), true);
    p->propProbe->properties.push_back(p->propSlState);
    p->GroupProperties.push_back(p->propProbe);
}

void PropsLightProbeUpdate(Properties* p, bool visible) {
    if (!p || !p->propProbe) return;
    // EL TRABAJO avanza una vez por cuadro (aunque haya varios paneles): ~25 ms por cuadro, el editor sigue andando
    if (RTProbeType() == 1 && gSlBaking) {
        static unsigned int last = 0;
        extern unsigned int w3dGetTicks();
        if (w3dGetTicks() - last >= 10) {
            last = w3dGetTicks();
            gSlProgress = RTProbeStep(25);
            if (!RTProbeActive()) Finish();
            g_redraw = true;
        }
    }
    LightProbe* s = visible ? SlActive() : NULL;
    p->propProbe->visible = (s != NULL);
    for (int k = 0; k < 3; k++) {
        if (p->propSlSize[k]) p->propSlSize[k]->value = s ? &s->size[kAxis[k]] : NULL;
        if (p->propSlPoints[k]) {
            p->propSlPoints[k]->value = s ? &gPoints[k] : NULL;
            if (s && g_propFloatEditing != p->propSlPoints[k]) gPoints[k] = (float)s->points[kAxis[k]];
        }
    }
    if (p->propSlRays) {
        p->propSlRays->value = s ? &gRays : NULL;
        if (s && g_propFloatEditing != p->propSlRays) gRays = (float)s->rays;
    }
    // MIENTRAS HORNEA (esta sonda): sin el boton, la barra y Cancelar
    const bool working = s && s == gSlBaking && RTProbeType() == 1;
    {
        bool change = false;
        const bool ocBake = (s == NULL) || working;
        if (p->propSlBake && p->propSlBake->hidden != ocBake) { p->propSlBake->hidden = ocBake; change = true; }
        if (p->propSlBar && p->propSlBar->hidden != !working) { p->propSlBar->hidden = !working; change = true; }
        if (p->propSlCancel && p->propSlCancel->hidden != !working) { p->propSlCancel->hidden = !working; change = true; }
        if (change) PropsLayoutDirty();
        if (working && p->propSlBar) {
            char b[32]; snprintf(b, sizeof(b), "%d%%", (int)(gSlProgress * 100.0f));
            p->propSlBar->button->text = b;
            p->propSlBar->button->bar = gSlProgress;
        }
    }
    if (p->propSlState) {
        p->propSlState->hidden = (s == NULL);
        std::string t;
        if (s && s->Baked()) { char b[96]; snprintf(b, sizeof(b), T("Baked: %d points"), s->Count()); t = b; }
        else t = T("Not baked");
        if (p->propSlState->name != t) { p->propSlState->name = t; PropsLayoutDirty(); }
    }
}
