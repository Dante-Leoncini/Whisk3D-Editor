// ============================================================================
//  PropsShadows.cpp — la tarjeta "Real-time shadows" de la pestania Render, debajo de la tarjeta Render
//  (render/RealTimeShadows.h). Apagada de fabrica: con el tilde aparecen las opciones, como el panel de sombras de
//  Unity: la distancia maxima, las cascadas y sus cortes, el borde donde se desvanecen, el desenfoque de las lejanas, la
//  resolucion, bordes suaves, la intensidad y el sesgo. Abajo, un aviso si no hay luz solar o si las hace la CPU.
//  Como PropsSondaLuz: el panel la ARMA (PropsShadowsBuild) y le pide el bindeo por frame (PropsShadowsUpdate).
// ============================================================================
#include "render/RayTracer.h"   // g_rt: con el trazado prendido la tarjeta no se muestra
#include "ViewPorts/Properties.h"
#include "WhiskUI/widgets/PopupMenu.h"   // el desplegable de la resolucion
#include "render/RealTimeShadows.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include <stdio.h>

extern void PropsLayoutDirty();   // (las filas cambiaron: rearmar la tarjeta)
void PropsOpenMenuUnderButton(PopupMenu* menu, Button* button);   // (Properties.cpp: el estandar de los desplegables)

// las copias de lo que el panel muestra distinto de como se guarda (enteros y porcentajes)
static float gCascades = 2.0f, gBlurPct = 0.0f, gIntPct = 100.0f;

static void Change() { g_redraw = true; }
static void ActionOn() { PropsLayoutDirty(); g_redraw = true; }
static void ActionCascades() {
    int n = (int)(gCascades + 0.5f); if (n < 1) n = 1; if (n > 4) n = 4;
    if (g_shadows.cascades != n) { g_shadows.cascades = n; PropsLayoutDirty(); }
    g_redraw = true;
}
static void ActionBlur() { g_shadows.blur = (gBlurPct < 0.0f ? 0.0f : gBlurPct) * 0.01f; g_redraw = true; }
static void ActionInt() { g_shadows.intensity = (gIntPct < 0.0f ? 0.0f : gIntPct) * 0.01f; g_redraw = true; }
// LA RESOLUCION: un desplegable con las de siempre (texeles de cada cascada)
static PopupMenu* gMenuRes = NULL;
static const int kRes[5] = { 256, 512, 1024, 2048, 4096 };
static void ActionPickedRes(int id) {
    if (id < 0 || id >= 5) return;
    g_shadows.resolution = kRes[id];
    g_redraw = true;
}
static void ActionMenuRes() {
    if (!PropsActive || !PropsActive->propShadowRes) return;
    if (!gMenuRes) { gMenuRes = new PopupMenu(); gMenuRes->action = ActionPickedRes; }
    gMenuRes->Clear();
    gMenuRes->title = T("Resolution");
    for (int i = 0; i < 5; i++) {
        char b[16]; snprintf(b, sizeof(b), "%d", kRes[i]);
        MenuItem* it = gMenuRes->Add(b, i);
        if (it) it->green = (g_shadows.resolution == kRes[i]);
    }
    PropsOpenMenuUnderButton(gMenuRes, PropsActive->propShadowRes->button);
}

static PropFloat* RowMeters(const char* name, float min, float max) {
    PropFloat* f = new PropFloat(T(name), "m");
    f->SetRange(min, max);
    f->fineStep = 0.5f; f->thickStep = 5.0f; f->dragStep = 0.1f;
    f->onChange = Change;
    return f;
}

void PropsShadowsBuild(Properties* p) {
    if (!p) return;
    p->propShadows = new GroupPropertie(T("Real-time shadows"));
    p->propShadowOn = new PropBool(T("Enabled"));   // (el titulo de la tarjeta ya dice que es)
    p->propShadowOn->value = &g_shadows.on;
    p->propShadowOn->onChange = ActionOn;
    p->propShadows->properties.push_back(p->propShadowOn);
    p->propShadowDist = RowMeters("Max distance", 0.5f, 100000.0f);
    p->propShadows->properties.push_back(p->propShadowDist);
    p->propShadowCascades = new PropFloat(T("Cascades"));
    p->propShadowCascades->SetRange(1.0f, 4.0f);
    p->propShadowCascades->integer = true;
    p->propShadowCascades->fineStep = 1.0f; p->propShadowCascades->thickStep = 1.0f; p->propShadowCascades->dragStep = 0.05f;
    p->propShadowCascades->onChange = ActionCascades;
    p->propShadows->properties.push_back(p->propShadowCascades);
    const char* cuts[3] = { "Split 1", "Split 2", "Split 3" };
    for (int i = 0; i < 3; i++) {
        p->propShadowCut[i] = RowMeters(cuts[i], 0.1f, 100000.0f);
        p->propShadows->properties.push_back(p->propShadowCut[i]);
    }
    p->propShadowBorder = RowMeters("Fade border", 0.0f, 100000.0f);
    p->propShadows->properties.push_back(p->propShadowBorder);
    p->propShadowBlur = new PropFloat(T("Penumbra"), "%");   // (la penumbra de contacto: nitida al pie, difusa lejos)
    p->propShadowBlur->SetRange(0.0f, 100.0f);
    p->propShadowBlur->integer = true;
    p->propShadowBlur->fineStep = 5.0f; p->propShadowBlur->thickStep = 25.0f; p->propShadowBlur->dragStep = 0.5f;
    p->propShadowBlur->onChange = ActionBlur;
    p->propShadows->properties.push_back(p->propShadowBlur);
    p->propShadowRes = new PropButton(T("Resolution"));
    p->propShadowRes->withLabel = true;
    p->propShadowRes->button->dropdown = true;
    p->propShadowRes->action = ActionMenuRes;
    p->propShadowRes->inColumn = true;   // (alineado con los numeros de la tarjeta)
    p->propShadows->properties.push_back(p->propShadowRes);
    p->propShadowSoft = new PropBool(T("Soft shadows"));
    p->propShadowSoft->onChange = Change;
    p->propShadows->properties.push_back(p->propShadowSoft);
    p->propShadowColor = new PropColor(T("Shadow color"));   // (negro = sin la luz del sol; un azul = la luz del cielo)
    p->propShadows->properties.push_back(p->propShadowColor);
    p->propShadowInt = new PropFloat(T("Shadow strength"), "%");
    p->propShadowInt->SetRange(0.0f, 100.0f);
    p->propShadowInt->integer = true;
    p->propShadowInt->fineStep = 5.0f; p->propShadowInt->thickStep = 25.0f; p->propShadowInt->dragStep = 0.5f;
    p->propShadowInt->onChange = ActionInt;
    p->propShadows->properties.push_back(p->propShadowInt);
    p->propShadowBias = new PropFloat(T("Bias"));
    p->propShadowBias->SetRange(0.0f, 10.0f);
    p->propShadowBias->fineStep = 0.1f; p->propShadowBias->thickStep = 1.0f; p->propShadowBias->dragStep = 0.01f;
    p->propShadowBias->onChange = Change;
    p->propShadows->properties.push_back(p->propShadowBias);
    p->propShadowState = new PropLabel("", true);
    p->propShadowState->hidden = true;
    p->propShadows->properties.push_back(p->propShadowState);
    p->GroupProperties.push_back(p->propShadows);
}

// (una fila de numero se muestra con valor y se esconde sin el: PropFloat sin valor no ocupa lugar)
static void Bind(PropFloat* f, float* v) {
    if (f && f->value != v) { f->value = v; PropsLayoutDirty(); }
}
static void BindB(PropBool* b, bool* v) {
    if (b && b->value != v) { b->value = v; PropsLayoutDirty(); }
}

void PropsShadowsUpdate(Properties* p, bool visible) {
    if (!p || !p->propShadows) return;
    // CON EL TRAZADO DE RAYOS prendido la tarjeta no esta: la vista Render y el render trazan sus propias sombras y estas
    // no se usan (siguen guardadas en el proyecto: el juego y las vistas sin trazado las usan)
    { extern W3dRTOptions g_rt; if (g_rt.on) visible = false; }
    if (p->propShadows->visible != visible) PropsLayoutDirty();
    p->propShadows->visible = visible;
    if (!visible) return;
    const bool on = g_shadows.on;
    const int n = g_shadows.cascades;
    if (g_propFloatEditing != p->propShadowCascades) gCascades = (float)n;
    if (g_propFloatEditing != p->propShadowBlur) gBlurPct = g_shadows.blur * 100.0f;
    if (g_propFloatEditing != p->propShadowInt) gIntPct = g_shadows.intensity * 100.0f;
    Bind(p->propShadowDist, on ? &g_shadows.distance : NULL);
    Bind(p->propShadowCascades, on ? &gCascades : NULL);
    for (int i = 0; i < 3; i++) Bind(p->propShadowCut[i], (on && i < n - 1) ? &g_shadows.cuts[i] : NULL);
    Bind(p->propShadowBorder, on ? &g_shadows.border : NULL);
    Bind(p->propShadowBlur, on ? &gBlurPct : NULL);
    BindB(p->propShadowSoft, on ? &g_shadows.smooth : NULL);
    Bind(p->propShadowInt, on ? &gIntPct : NULL);
    if (p->propShadowColor && p->propShadowColor->value != (on ? g_shadows.color : NULL)) { p->propShadowColor->value = on ? g_shadows.color : NULL; PropsLayoutDirty(); }
    Bind(p->propShadowBias, on ? &g_shadows.bias : NULL);
    if (p->propShadowRes) {
        if (p->propShadowRes->hidden != !on) { p->propShadowRes->hidden = !on; PropsLayoutDirty(); }
        char b[16]; snprintf(b, sizeof(b), "%d", g_shadows.resolution);
        if (p->propShadowRes->button->text != b) { p->propShadowRes->button->text = b; g_redraw = true; }
    }
    // EL AVISO: sin luz solar no hay sombras; sin shaders las hace la CPU (no en tiempo real)
    if (p->propShadowState) {
        std::string t;
        if (on && W3dShadowsState() == 1) t = T("No sun light in the scene: shadows come from the first sun light");
        else if (on && !W3dShadowsWithShaders()) t = T("OpenGL ES 1.1: the GPU and the CPU compute them (not real time)");
        const bool hidden = t.empty();
        if (p->propShadowState->hidden != hidden || p->propShadowState->name != t) {
            p->propShadowState->hidden = hidden; p->propShadowState->name = t; PropsLayoutDirty();
        }
    }
}
