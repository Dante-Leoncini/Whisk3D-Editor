// ============================================================================
//  PropsSombras.cpp — la tarjeta "Real-time shadows" de la pestania Render, debajo de la tarjeta Render
//  (render/SombrasTiempoReal.h). Apagada de fabrica: con el tilde aparecen las opciones, como el panel de sombras de
//  Unity: la distancia maxima, las cascadas y sus cortes, el borde donde se desvanecen, el desenfoque de las lejanas, la
//  resolucion, bordes suaves, la intensidad y el sesgo. Abajo, un aviso si no hay luz solar o si las hace la CPU.
//  Como PropsSondaLuz: el panel la ARMA (PropsSombrasConstruir) y le pide el bindeo por frame (PropsSombrasActualizar).
// ============================================================================
#include "ViewPorts/Properties.h"
#include "WhiskUI/widgets/PopupMenu.h"   // el desplegable de la resolucion
#include "render/SombrasTiempoReal.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include <stdio.h>

extern void PropsLayoutSucio();   // (las filas cambiaron: rearmar la tarjeta)
void PropsAbrirMenuBajoBoton(PopupMenu* menu, Button* boton);   // (Properties.cpp: el estandar de los desplegables)

// las copias de lo que el panel muestra distinto de como se guarda (enteros y porcentajes)
static float gCascadas = 2.0f, gDesenfPct = 0.0f, gIntPct = 100.0f;

static void Cambio() { g_redraw = true; }
static void AccionOn() { PropsLayoutSucio(); g_redraw = true; }
static void AccionCascadas() {
    int n = (int)(gCascadas + 0.5f); if (n < 1) n = 1; if (n > 4) n = 4;
    if (g_sombras.cascadas != n) { g_sombras.cascadas = n; PropsLayoutSucio(); }
    g_redraw = true;
}
static void AccionDesenf() { g_sombras.desenfoque = (gDesenfPct < 0.0f ? 0.0f : gDesenfPct) * 0.01f; g_redraw = true; }
static void AccionInt() { g_sombras.intensidad = (gIntPct < 0.0f ? 0.0f : gIntPct) * 0.01f; g_redraw = true; }
// LA RESOLUCION: un desplegable con las de siempre (texeles de cada cascada)
static PopupMenu* gMenuRes = NULL;
static const int kRes[5] = { 256, 512, 1024, 2048, 4096 };
static void AccionResElegida(int id) {
    if (id < 0 || id >= 5) return;
    g_sombras.resolucion = kRes[id];
    g_redraw = true;
}
static void AccionMenuRes() {
    if (!PropsActivo || !PropsActivo->propSomRes) return;
    if (!gMenuRes) { gMenuRes = new PopupMenu(); gMenuRes->action = AccionResElegida; }
    gMenuRes->Limpiar();
    gMenuRes->titulo = T("Resolution");
    for (int i = 0; i < 5; i++) {
        char b[16]; snprintf(b, sizeof(b), "%d", kRes[i]);
        MenuItem* it = gMenuRes->Agregar(b, i);
        if (it) it->verde = (g_sombras.resolucion == kRes[i]);
    }
    PropsAbrirMenuBajoBoton(gMenuRes, PropsActivo->propSomRes->button);
}

static PropFloat* FilaMetros(const char* nombre, float minimo, float maximo) {
    PropFloat* f = new PropFloat(T(nombre), "m");
    f->SetRango(minimo, maximo);
    f->stepFino = 0.5f; f->stepGrueso = 5.0f; f->dragStep = 0.1f;
    f->onChange = Cambio;
    return f;
}

void PropsSombrasConstruir(Properties* p) {
    if (!p) return;
    p->propSombras = new GroupPropertie(T("Real-time shadows"));
    p->propSomOn = new PropBool(T("Enabled"));   // (el titulo de la tarjeta ya dice que es)
    p->propSomOn->value = &g_sombras.on;
    p->propSomOn->onChange = AccionOn;
    p->propSombras->properties.push_back(p->propSomOn);
    p->propSomDist = FilaMetros("Max distance", 0.5f, 100000.0f);
    p->propSombras->properties.push_back(p->propSomDist);
    p->propSomCascadas = new PropFloat(T("Cascades"));
    p->propSomCascadas->SetRango(1.0f, 4.0f);
    p->propSomCascadas->entero = true;
    p->propSomCascadas->stepFino = 1.0f; p->propSomCascadas->stepGrueso = 1.0f; p->propSomCascadas->dragStep = 0.05f;
    p->propSomCascadas->onChange = AccionCascadas;
    p->propSombras->properties.push_back(p->propSomCascadas);
    const char* cortes[3] = { "Split 1", "Split 2", "Split 3" };
    for (int i = 0; i < 3; i++) {
        p->propSomCorte[i] = FilaMetros(cortes[i], 0.1f, 100000.0f);
        p->propSombras->properties.push_back(p->propSomCorte[i]);
    }
    p->propSomBorde = FilaMetros("Fade border", 0.0f, 100000.0f);
    p->propSombras->properties.push_back(p->propSomBorde);
    p->propSomDesenf = new PropFloat(T("Far shadow blur"), "%");
    p->propSomDesenf->SetRango(0.0f, 100.0f);
    p->propSomDesenf->entero = true;
    p->propSomDesenf->stepFino = 5.0f; p->propSomDesenf->stepGrueso = 25.0f; p->propSomDesenf->dragStep = 0.5f;
    p->propSomDesenf->onChange = AccionDesenf;
    p->propSombras->properties.push_back(p->propSomDesenf);
    p->propSomRes = new PropButton(T("Resolution"));
    p->propSomRes->conLabel = true;
    p->propSomRes->button->desplegable = true;
    p->propSomRes->action = AccionMenuRes;
    p->propSomRes->enColumna = true;   // (alineado con los numeros de la tarjeta)
    p->propSombras->properties.push_back(p->propSomRes);
    p->propSomSuaves = new PropBool(T("Soft shadows"));
    p->propSomSuaves->onChange = Cambio;
    p->propSombras->properties.push_back(p->propSomSuaves);
    p->propSomInt = new PropFloat(T("Shadow strength"), "%");
    p->propSomInt->SetRango(0.0f, 100.0f);
    p->propSomInt->entero = true;
    p->propSomInt->stepFino = 5.0f; p->propSomInt->stepGrueso = 25.0f; p->propSomInt->dragStep = 0.5f;
    p->propSomInt->onChange = AccionInt;
    p->propSombras->properties.push_back(p->propSomInt);
    p->propSomSesgo = new PropFloat(T("Bias"));
    p->propSomSesgo->SetRango(0.0f, 10.0f);
    p->propSomSesgo->stepFino = 0.1f; p->propSomSesgo->stepGrueso = 1.0f; p->propSomSesgo->dragStep = 0.01f;
    p->propSomSesgo->onChange = Cambio;
    p->propSombras->properties.push_back(p->propSomSesgo);
    p->propSomEstado = new PropLabel("", true);
    p->propSomEstado->oculto = true;
    p->propSombras->properties.push_back(p->propSomEstado);
    p->GroupProperties.push_back(p->propSombras);
}

// (una fila de numero se muestra con valor y se esconde sin el: PropFloat sin valor no ocupa lugar)
static void Bindear(PropFloat* f, float* v) {
    if (f && f->value != v) { f->value = v; PropsLayoutSucio(); }
}
static void BindearB(PropBool* b, bool* v) {
    if (b && b->value != v) { b->value = v; PropsLayoutSucio(); }
}

void PropsSombrasActualizar(Properties* p, bool visible) {
    if (!p || !p->propSombras) return;
    p->propSombras->visible = visible;
    if (!visible) return;
    const bool on = g_sombras.on;
    const int n = g_sombras.cascadas;
    if (g_propFloatEditando != p->propSomCascadas) gCascadas = (float)n;
    if (g_propFloatEditando != p->propSomDesenf) gDesenfPct = g_sombras.desenfoque * 100.0f;
    if (g_propFloatEditando != p->propSomInt) gIntPct = g_sombras.intensidad * 100.0f;
    Bindear(p->propSomDist, on ? &g_sombras.distancia : NULL);
    Bindear(p->propSomCascadas, on ? &gCascadas : NULL);
    for (int i = 0; i < 3; i++) Bindear(p->propSomCorte[i], (on && i < n - 1) ? &g_sombras.cortes[i] : NULL);
    Bindear(p->propSomBorde, on ? &g_sombras.borde : NULL);
    Bindear(p->propSomDesenf, on ? &gDesenfPct : NULL);
    BindearB(p->propSomSuaves, on ? &g_sombras.suaves : NULL);
    Bindear(p->propSomInt, on ? &gIntPct : NULL);
    Bindear(p->propSomSesgo, on ? &g_sombras.sesgo : NULL);
    if (p->propSomRes) {
        if (p->propSomRes->oculto != !on) { p->propSomRes->oculto = !on; PropsLayoutSucio(); }
        char b[16]; snprintf(b, sizeof(b), "%d", g_sombras.resolucion);
        if (p->propSomRes->button->text != b) { p->propSomRes->button->text = b; g_redraw = true; }
    }
    // EL AVISO: sin luz solar no hay sombras; sin shaders las hace la CPU (no en tiempo real)
    if (p->propSomEstado) {
        std::string t;
        if (on && W3dSombrasEstado() == 1) t = T("No sun light in the scene: shadows come from the first sun light");
        else if (on && !W3dSombrasConShaders()) t = T("OpenGL ES 1.1: the GPU and the CPU compute them (not real time)");
        const bool oculto = t.empty();
        if (p->propSomEstado->oculto != oculto || p->propSomEstado->name != t) {
            p->propSomEstado->oculto = oculto; p->propSomEstado->name = t; PropsLayoutSucio();
        }
    }
}
