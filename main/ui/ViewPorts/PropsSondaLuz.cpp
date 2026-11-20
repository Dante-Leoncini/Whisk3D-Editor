// ============================================================================
//  PropsSondaLuz.cpp — la tarjeta "Light Probe" del panel de propiedades (pestania 2 de la sonda de luz,
//  objects/SondaLuz.h). Como PropsHitbox: el panel la ARMA (PropsSondaLuzConstruir) y le pide el bindeo por frame
//  (PropsSondaLuzActualizar).
//
//  El tamano bindea directo; los PUNTOS y los RAYOS son enteros (filas de numero con su copia en float): cambiar los
//  puntos BORRA lo horneado (la grilla ya no es la misma). "Bake" hornea con el trazado de rayos de la CPU.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/SondaLuz.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "render/RayTracer.h"   // RTSonda*: hornear de a pedazos (la barra)
#include <stdio.h>

extern void Notificar(const std::string& texto, bool error);
extern void PropsLayoutSucio();   // (el estado cambio de largo: rearmar la tarjeta)

static SondaLuz* SlActiva() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::sondaluz) ? (SondaLuz*)ObjActivo : NULL;
}
// Z-ARRIBA, como la tarjeta Transformar: el campo "Y" es la z interna y el "Z" la y
static const int kEje[3] = { 0, 2, 1 };
static float gPuntos[3] = { 3, 3, 3 };
static float gRayos = 256;

static void AccionPuntos() {
    SondaLuz* s = SlActiva();
    if (!s) return;
    bool cambio = false;
    for (int k = 0; k < 3; k++) {
        int n = (int)(gPuntos[k] + 0.5f); if (n < 2) n = 2; if (n > 16) n = 16;
        if (s->puntos[kEje[k]] != n) { s->puntos[kEje[k]] = n; cambio = true; }
    }
    if (cambio) s->sh.clear();   // (la grilla es otra: lo horneado no vale)
    g_redraw = true;
}
static void AccionRayos() {
    SondaLuz* s = SlActiva();
    if (!s) return;
    int n = (int)(gRayos + 0.5f); if (n < 16) n = 16; if (n > 8192) n = 8192;
    s->rayos = n;
}
// HORNEAR DE A PEDAZOS: la barra del progreso y Cancelar, como el horneado de la luz (PropsHorneado.cpp). El trabajo
// avanza en PropsSondaLuzActualizar, una vez por cuadro
static SondaLuz* gSlHorneando = NULL;
static float gSlProgreso = 0.0f;
static void AccionCancelar() {
    if (RTSondaTipo() != 1) return;
    RTSondaCancelar(); gSlHorneando = NULL;
    Notificar(T("Bake canceled"), true); PropsLayoutSucio(); g_redraw = true;
}
static void AccionHornear() {
    if (RTSondaTipo() == 1) { AccionCancelar(); return; }
    SondaLuz* s = SlActiva();
    if (!s) return;
    if (RTSondaActiva()) { Notificar(T("Another probe is baking"), true); return; }
    std::string motivo;
    if (!SondaLuzHornearEmpezar(s, &motivo)) { Notificar(std::string(T("Not baked")) + ": " + T(motivo.c_str()), true); return; }
    gSlHorneando = s; gSlProgreso = 0.0f;
    PropsLayoutSucio();
    g_redraw = true;
}
static void Terminar() {
    SondaLuz* s = gSlHorneando; gSlHorneando = NULL;
    if (!s || !SondaLuzViva(s) || !SondaLuzHornearTerminar(s)) { RTSondaCancelar(); Notificar(T("Not baked"), true); }
    else { char b[96]; snprintf(b, sizeof(b), T("Light probe baked: %d points"), s->Cantidad()); Notificar(b, false); }
    PropsLayoutSucio();
    g_redraw = true;
}

void PropsSondaLuzConstruir(Properties* p) {
    if (!p) return;
    p->propSonda = new GroupPropertie(T("Light Probe"));
    const char* ejes[3] = { "Size X", "Size Y", "Size Z" };
    for (int k = 0; k < 3; k++) {
        p->propSlTam[k] = new PropFloat(T(ejes[k]), "m");
        p->propSlTam[k]->SetRango(0.01f, 100000.0f);
        p->propSlTam[k]->stepFino = 0.1f; p->propSlTam[k]->stepGrueso = 1.0f;
        p->propSonda->properties.push_back(p->propSlTam[k]);
    }
    const char* pts[3] = { "Points X", "Points Y", "Points Z" };
    for (int k = 0; k < 3; k++) {
        p->propSlPuntos[k] = new PropFloat(T(pts[k]));
        p->propSlPuntos[k]->SetRango(2.0f, 16.0f);
        p->propSlPuntos[k]->entero = true;
        p->propSlPuntos[k]->stepFino = 1.0f; p->propSlPuntos[k]->stepGrueso = 1.0f; p->propSlPuntos[k]->dragStep = 0.05f;
        p->propSlPuntos[k]->onChange = AccionPuntos;
        p->propSonda->properties.push_back(p->propSlPuntos[k]);
    }
    p->propSlRayos = new PropFloat(T("Rays"));
    p->propSlRayos->SetRango(16.0f, 8192.0f);
    p->propSlRayos->entero = true;
    p->propSlRayos->stepFino = 16.0f; p->propSlRayos->stepGrueso = 128.0f; p->propSlRayos->dragStep = 2.0f;
    p->propSlRayos->onChange = AccionRayos;
    p->propSonda->properties.push_back(p->propSlRayos);
    p->propSlHornear = new PropButton(T("Bake"), IconType::esfera);
    p->propSlHornear->action = AccionHornear;
    p->propSonda->properties.push_back(p->propSlHornear);
    p->propSlBarra = new PropButton("0%");   // (la barra del progreso, solo mientras hornea)
    p->propSlBarra->button->barra = 0.0f;
    p->propSlBarra->oculto = true;
    p->propSonda->properties.push_back(p->propSlBarra);
    p->propSlCancelar = new PropButton(T("Cancel"), IconType::cerrar);
    p->propSlCancelar->action = AccionCancelar;
    p->propSlCancelar->oculto = true;
    p->propSonda->properties.push_back(p->propSlCancelar);
    p->propSlEstado = new PropLabel(T("Not baked"), true);
    p->propSonda->properties.push_back(p->propSlEstado);
    p->GroupProperties.push_back(p->propSonda);
}

void PropsSondaLuzActualizar(Properties* p, bool visible) {
    if (!p || !p->propSonda) return;
    // EL TRABAJO avanza una vez por cuadro (aunque haya varios paneles): ~25 ms por cuadro, el editor sigue andando
    if (RTSondaTipo() == 1 && gSlHorneando) {
        static unsigned int ultimo = 0;
        extern unsigned int w3dGetTicks();
        if (w3dGetTicks() - ultimo >= 10) {
            ultimo = w3dGetTicks();
            gSlProgreso = RTSondaPaso(25);
            if (!RTSondaActiva()) Terminar();
            g_redraw = true;
        }
    }
    SondaLuz* s = visible ? SlActiva() : NULL;
    p->propSonda->visible = (s != NULL);
    for (int k = 0; k < 3; k++) {
        if (p->propSlTam[k]) p->propSlTam[k]->value = s ? &s->tam[kEje[k]] : NULL;
        if (p->propSlPuntos[k]) {
            p->propSlPuntos[k]->value = s ? &gPuntos[k] : NULL;
            if (s && g_propFloatEditando != p->propSlPuntos[k]) gPuntos[k] = (float)s->puntos[kEje[k]];
        }
    }
    if (p->propSlRayos) {
        p->propSlRayos->value = s ? &gRayos : NULL;
        if (s && g_propFloatEditando != p->propSlRayos) gRayos = (float)s->rayos;
    }
    // MIENTRAS HORNEA (esta sonda): sin el boton, la barra y Cancelar
    const bool trabajando = s && s == gSlHorneando && RTSondaTipo() == 1;
    {
        bool cambio = false;
        const bool ocHornear = (s == NULL) || trabajando;
        if (p->propSlHornear && p->propSlHornear->oculto != ocHornear) { p->propSlHornear->oculto = ocHornear; cambio = true; }
        if (p->propSlBarra && p->propSlBarra->oculto != !trabajando) { p->propSlBarra->oculto = !trabajando; cambio = true; }
        if (p->propSlCancelar && p->propSlCancelar->oculto != !trabajando) { p->propSlCancelar->oculto = !trabajando; cambio = true; }
        if (cambio) PropsLayoutSucio();
        if (trabajando && p->propSlBarra) {
            char b[32]; snprintf(b, sizeof(b), "%d%%", (int)(gSlProgreso * 100.0f));
            p->propSlBarra->button->text = b;
            p->propSlBarra->button->barra = gSlProgreso;
        }
    }
    if (p->propSlEstado) {
        p->propSlEstado->oculto = (s == NULL);
        std::string t;
        if (s && s->Horneada()) { char b[96]; snprintf(b, sizeof(b), T("Baked: %d points"), s->Cantidad()); t = b; }
        else t = T("Not baked");
        if (p->propSlEstado->name != t) { p->propSlEstado->name = t; PropsLayoutSucio(); }
    }
}
