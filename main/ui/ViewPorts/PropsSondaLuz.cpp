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
static void AccionHornear() {
    SondaLuz* s = SlActiva();
    if (!s) return;
    std::string motivo;
    if (!SondaLuzHornear(s, &motivo)) { Notificar(std::string(T("Not baked")) + ": " + T(motivo.c_str()), true); return; }
    char b[96]; snprintf(b, sizeof(b), T("Light probe baked: %d points"), s->Cantidad());
    Notificar(b, false);
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
    p->propSlEstado = new PropLabel(T("Not baked"), true);
    p->propSonda->properties.push_back(p->propSlEstado);
    p->GroupProperties.push_back(p->propSonda);
}

void PropsSondaLuzActualizar(Properties* p, bool visible) {
    if (!p || !p->propSonda) return;
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
    if (p->propSlHornear) p->propSlHornear->oculto = (s == NULL);
    if (p->propSlEstado) {
        p->propSlEstado->oculto = (s == NULL);
        std::string t;
        if (s && s->Horneada()) { char b[96]; snprintf(b, sizeof(b), T("Baked: %d points"), s->Cantidad()); t = b; }
        else t = T("Not baked");
        if (p->propSlEstado->name != t) { p->propSlEstado->name = t; PropsLayoutSucio(); }
    }
}
