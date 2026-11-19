// ============================================================================
//  PropsSondaReflejo.cpp — la tarjeta "Reflection Probe" del panel de propiedades (pestania 2 de la sonda de reflejo,
//  objects/SondaReflejo.h). Como PropsSondaLuz: el panel la ARMA (PropsSondaReflejoConstruir) y le pide el bindeo por
//  frame (PropsSondaReflejoActualizar).
//
//    Tipo (Horneada / Personalizada / Tiempo real), Importancia, Intensidad, Proyeccion en caja, el tamanio de la caja y el
//    origen de la captura; Resolucion, Cerca; la imagen (la elegida, en una personalizada). La HORNEADA (trazado de
//    rayos): Muestras, Rebotes y Hornear. La de TIEMPO REAL (la dibuja la GPU): cada cuanto se vuelve a dibujar. El fondo
//    es siempre el de la escena
// ============================================================================
#include "ViewPorts/Properties.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "objects/SondaReflejo.h"
#include "objects/Textures.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include <stdio.h>

extern void Notificar(const std::string& texto, bool error);
extern void PropsLayoutSucio();
void PropsAbrirMenuBajoBoton(PopupMenu* menu, Button* boton);   // (Properties.cpp: el estandar de los desplegables)

static SondaReflejo* SrActiva() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::sondareflejo) ? (SondaReflejo*)ObjActivo : NULL;
}
// Z-ARRIBA, como la tarjeta Transformar: el campo "Y" es la z interna y el "Z" la y
static const int kEje[3] = { 0, 2, 1 };
static float gImportancia = 1, gRes = 256, gMuestras = 32, gRebotes = 3;

static void Cambio() { g_redraw = true; }
static void AccionImportancia() { SondaReflejo* s = SrActiva(); if (s) s->importancia = (int)(gImportancia + (gImportancia >= 0 ? 0.5f : -0.5f)); }
static void AccionMuestras() { SondaReflejo* s = SrActiva(); if (s) s->muestras = gMuestras < 1.0f ? 1 : (int)(gMuestras + 0.5f); }
static void AccionRebotes() { SondaReflejo* s = SrActiva(); if (s) s->rebotes = gRebotes < 0.0f ? 0 : (int)(gRebotes + 0.5f); }

// ---- el TIPO ----
static PopupMenu* gMenuTipo = NULL;
static const char* kTipos[3] = { "Baked", "Custom", "Realtime" };
static void AccionTipoElegido(int id) {
    SondaReflejo* s = SrActiva();
    if (!s || id < 0 || id > 2) return;
    s->tipo = id;
    PropsLayoutSucio(); g_redraw = true;
}
static void AccionMenuTipo() {
    SondaReflejo* s = SrActiva();
    if (!s || !PropsActivo || !PropsActivo->propSrTipo) return;
    if (!gMenuTipo) { gMenuTipo = new PopupMenu(); gMenuTipo->action = AccionTipoElegido; }
    gMenuTipo->Limpiar();
    gMenuTipo->titulo = T("Type");
    for (int i = 0; i < 3; i++) { MenuItem* it = gMenuTipo->Agregar(T(kTipos[i]), i); if (it) it->verde = (s->tipo == i); }
    PropsAbrirMenuBajoBoton(gMenuTipo, PropsActivo->propSrTipo->button);
}
// ---- la RESOLUCION (el ancho del 360) ----
static PopupMenu* gMenuRes = NULL;
static const int kRes[5] = { 64, 128, 256, 512, 1024 };
static void AccionResElegida(int id) { SondaReflejo* s = SrActiva(); if (s && id >= 0 && id < 5) s->resolucion = kRes[id]; g_redraw = true; }
static void AccionMenuRes() {
    SondaReflejo* s = SrActiva();
    if (!s || !PropsActivo || !PropsActivo->propSrRes) return;
    if (!gMenuRes) { gMenuRes = new PopupMenu(); gMenuRes->action = AccionResElegida; }
    gMenuRes->Limpiar();
    gMenuRes->titulo = T("Resolution");
    for (int i = 0; i < 5; i++) {
        char b[24]; snprintf(b, sizeof(b), "%d x %d", kRes[i], kRes[i] / 2);
        MenuItem* it = gMenuRes->Agregar(b, i); if (it) it->verde = (s->resolucion == kRes[i]);
    }
    PropsAbrirMenuBajoBoton(gMenuRes, PropsActivo->propSrRes->button);
}
// ---- la IMAGEN de una personalizada: una textura del proyecto (una 360) ----
static PopupMenu* gMenuImg = NULL;
static void AccionImgElegida(int id) {
    SondaReflejo* s = SrActiva();
    if (!s) return;
    if (id == 0) s->textura.clear();
    else { const size_t i = (size_t)(id - 1) + 5; if (i < Textures.size() && Textures[i]) s->textura = Textures[i]->path; }
    s->Imagen();
    PropsLayoutSucio(); g_redraw = true;
}
static void AccionMenuImg() {
    SondaReflejo* s = SrActiva();
    if (!s || !PropsActivo || !PropsActivo->propSrImagen) return;
    if (!gMenuImg) { gMenuImg = new PopupMenu(); gMenuImg->action = AccionImgElegida; }
    gMenuImg->Limpiar();
    gMenuImg->titulo = T("360 Image");
    gMenuImg->Agregar(T("None"), 0);
    for (size_t i = 5; i < Textures.size(); i++) {
        if (!Textures[i]) continue;
        std::string n = Textures[i]->path;
        const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
        MenuItem* it = gMenuImg->Agregar(n, 1 + (int)(i - 5), IconType::textura);
        if (it) it->verde = (Textures[i]->path == s->textura);
    }
    PropsAbrirMenuBajoBoton(gMenuImg, PropsActivo->propSrImagen->button);
}
static void AccionHornear() {
    SondaReflejo* s = SrActiva();
    if (!s) return;
    std::string motivo;
    extern unsigned int w3dGetTicks();
    const unsigned t0 = w3dGetTicks();
    if (!SondaReflejoHornear(s, &motivo)) { Notificar(std::string(T("Not baked")) + ": " + T(motivo.c_str()), true); return; }
    char b[96]; snprintf(b, sizeof(b), T("Reflection probe baked (%u ms)"), w3dGetTicks() - t0);
    Notificar(b, false);
    PropsLayoutSucio();
    g_redraw = true;
}

static PropFloat* Fila(const char* nombre, const char* unidad, float mn, float mx, float fino, float grueso) {
    PropFloat* f = new PropFloat(T(nombre), unidad);
    f->SetRango(mn, mx);
    f->stepFino = fino; f->stepGrueso = grueso; f->dragStep = fino * 0.1f;
    f->onChange = Cambio;
    return f;
}
static PropButton* Desplegable(const char* nombre, void (*accion)()) {
    PropButton* b = new PropButton(T(nombre));
    b->conLabel = true; b->enColumna = true;
    b->button->desplegable = true;
    b->action = accion;
    return b;
}

void PropsSondaReflejoConstruir(Properties* p) {
    if (!p) return;
    p->propSondaRef = new GroupPropertie(T("Reflection Probe"));
    p->propSrTipo = Desplegable("Type", AccionMenuTipo);
    p->propSondaRef->properties.push_back(p->propSrTipo);
    p->propSrImportancia = Fila("Importance", "", -100.0f, 100.0f, 1.0f, 10.0f);
    p->propSrImportancia->entero = true;
    p->propSrImportancia->onChange = AccionImportancia;
    p->propSondaRef->properties.push_back(p->propSrImportancia);
    p->propSrIntensidad = Fila("Intensity", "", 0.0f, 16.0f, 0.1f, 1.0f);
    p->propSondaRef->properties.push_back(p->propSrIntensidad);
    p->propSrCaja = new PropBool(T("Box Projection"));
    p->propSrCaja->onChange = Cambio;
    p->propSondaRef->properties.push_back(p->propSrCaja);
    const char* tams[3] = { "Size X", "Size Y", "Size Z" };
    for (int k = 0; k < 3; k++) { p->propSrTam[k] = Fila(tams[k], "m", 0.01f, 100000.0f, 0.1f, 1.0f); p->propSondaRef->properties.push_back(p->propSrTam[k]); }
    const char* ors[3] = { "Probe Origin X", "Probe Origin Y", "Probe Origin Z" };
    for (int k = 0; k < 3; k++) { p->propSrOrigen[k] = Fila(ors[k], "m", -100000.0f, 100000.0f, 0.1f, 1.0f); p->propSondaRef->properties.push_back(p->propSrOrigen[k]); }
    p->propSrRes = Desplegable("Resolution", AccionMenuRes);
    p->propSondaRef->properties.push_back(p->propSrRes);
    p->propSrCerca = Fila("Near", "m", 0.0f, 1000.0f, 0.05f, 0.5f);
    p->propSondaRef->properties.push_back(p->propSrCerca);
    p->propSrMuestras = Fila("Samples", "", 1.0f, 1024.0f, 1.0f, 8.0f);
    p->propSrMuestras->entero = true;
    p->propSrMuestras->onChange = AccionMuestras;
    p->propSondaRef->properties.push_back(p->propSrMuestras);
    p->propSrRebotes = Fila("Bounces", "", 0.0f, 8.0f, 1.0f, 1.0f);
    p->propSrRebotes->entero = true;
    p->propSrRebotes->onChange = AccionRebotes;
    p->propSondaRef->properties.push_back(p->propSrRebotes);
    p->propSrCada = Fila("Update every", "s", 0.0f, 600.0f, 0.1f, 1.0f);
    p->propSondaRef->properties.push_back(p->propSrCada);
    p->propSrImagen = Desplegable("360 Image", AccionMenuImg);
    p->propSondaRef->properties.push_back(p->propSrImagen);
    p->propSrHornear = new PropButton(T("Bake"), IconType::foto);
    p->propSrHornear->action = AccionHornear;
    p->propSondaRef->properties.push_back(p->propSrHornear);
    p->propSrEstado = new PropLabel(T("Not baked"), true);
    p->propSondaRef->properties.push_back(p->propSrEstado);
    p->GroupProperties.push_back(p->propSondaRef);
}

static void BindF(PropFloat* f, float* v) { if (f && f->value != v) { f->value = v; PropsLayoutSucio(); } }
static void BindB(PropBool* b, bool* v) { if (b && b->value != v) { b->value = v; PropsLayoutSucio(); } }
static void Ocultar(PropButton* b, bool o) { if (b && b->oculto != o) { b->oculto = o; PropsLayoutSucio(); } }

void PropsSondaReflejoActualizar(Properties* p, bool visible) {
    if (!p || !p->propSondaRef) return;
    SondaReflejo* s = visible ? SrActiva() : NULL;
    p->propSondaRef->visible = (s != NULL);
    if (!s) {
        BindF(p->propSrImportancia, NULL); BindF(p->propSrIntensidad, NULL); BindB(p->propSrCaja, NULL);
        for (int k = 0; k < 3; k++) { BindF(p->propSrTam[k], NULL); BindF(p->propSrOrigen[k], NULL); }
        BindF(p->propSrCerca, NULL); BindF(p->propSrCada, NULL);
        BindF(p->propSrMuestras, NULL); BindF(p->propSrRebotes, NULL);
        return;
    }
    const bool horneable = s->tipo != W3D_REFLEJO_PERSONALIZADA;
    if (p->propSrTipo) { const std::string t = T(kTipos[s->tipo < 0 || s->tipo > 2 ? 0 : s->tipo]);
                         if (p->propSrTipo->button->text != t) { p->propSrTipo->button->text = t; g_redraw = true; } }
    if (g_propFloatEditando != p->propSrImportancia) gImportancia = (float)s->importancia;
    BindF(p->propSrImportancia, &gImportancia);
    BindF(p->propSrIntensidad, horneable ? &s->intensidad : NULL);
    BindB(p->propSrCaja, &s->caja);
    for (int k = 0; k < 3; k++) { BindF(p->propSrTam[k], &s->tam[kEje[k]]); BindF(p->propSrOrigen[k], &s->origen[kEje[k]]); }
    Ocultar(p->propSrRes, !horneable);
    if (p->propSrRes) { char b[24]; snprintf(b, sizeof(b), "%d x %d", s->resolucion, s->resolucion / 2);
                        if (p->propSrRes->button->text != b) { p->propSrRes->button->text = b; g_redraw = true; } }
    BindF(p->propSrCerca, horneable ? &s->cerca : NULL);
    BindF(p->propSrCada, s->tipo == W3D_REFLEJO_TIEMPO_REAL ? &s->cadaSeg : NULL);
    // (las del trazado de rayos: solo la horneada)
    const bool horneada = s->tipo == W3D_REFLEJO_HORNEADA;
    if (g_propFloatEditando != p->propSrMuestras) gMuestras = (float)s->muestras;
    if (g_propFloatEditando != p->propSrRebotes) gRebotes = (float)s->rebotes;
    BindF(p->propSrMuestras, horneada ? &gMuestras : NULL);
    BindF(p->propSrRebotes, horneada ? &gRebotes : NULL);
    Ocultar(p->propSrImagen, horneable);
    if (p->propSrImagen) {
        std::string n = s->textura.empty() ? std::string(T("None")) : s->textura;
        const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
        if (p->propSrImagen->button->text != n) { p->propSrImagen->button->text = n; g_redraw = true; }
    }
    // HORNEAR y su estado: solo la horneada (la de tiempo real la dibuja la GPU; la personalizada usa su imagen)
    const bool vivo = s->tipo == W3D_REFLEJO_TIEMPO_REAL;
    Ocultar(p->propSrHornear, !horneable || vivo);
    if (p->propSrEstado) {
        const bool oc = vivo || s->tipo == W3D_REFLEJO_PERSONALIZADA;
        if (p->propSrEstado->oculto != oc) { p->propSrEstado->oculto = oc; PropsLayoutSucio(); }
        std::string t;
        if (s->textura.empty()) t = T("Not baked");
        else { std::string n = s->textura; const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
               t = std::string(T("Baked")) + ": " + n; }
        if (p->propSrEstado->name != t) { p->propSrEstado->name = t; PropsLayoutSucio(); }
    }
}
