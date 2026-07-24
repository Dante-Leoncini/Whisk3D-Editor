// ============================================================================
//  SimJuego.cpp — ver SimJuego.h.
// ============================================================================
#include "script/SimJuego.h"
#include "script/W3dScript.h"
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Elemento2D.h"
#include "objects/Texto2D.h"
#include "render/UIOverlay.h"
#include "render/OpcionesRender.h"     // g_redraw
#include "animation/Animation.h"       // CurrentFrame / StartFrame / EndFrame / PlayAnimation
#include "objects/Gamepad.h"           // axisState / buttonState (el pad SDL del editor)
#include <SDL.h>
#include <vector>
#include <string>

// ---- SNAPSHOT del estado (lo que un script puede tocar hoy) ----------------
struct SimEnt {
    Object* o;
    float px, py, pz;
    float rot2d, ancho, alto, scrollX, scrollY, opacidad;
    bool  es2d, esTexto;
    std::string texto;
};
struct SimSnap { std::vector<SimEnt> ents; };

static bool gActiva = false;
static SimSnap gBase;
static std::vector<SimSnap> gGrab;    // un snapshot por frame simulado (editor)
static int gTick = 0;
static std::vector<Object*> gScripted;
static int gGrabOffset = 0;   // tick ABSOLUTO del primer snapshot (el cache rueda)
int gSimCacheMax = 250;       // techo del cache (frames), configurable (tarjeta Juego)

bool SimActiva() { return gActiva; }
int  SimFramesGrabados() { return (int)gGrab.size(); }
int  SimFrameActual() { return gTick; }
int  SimPrimerFrame() { return gGrabOffset; }

static void Recorrer(Object* o, SimSnap* snap, std::vector<Object*>* scripted) {
    if (!o) return;
    if (snap) {
        SimEnt e;
        e.o = o;
        e.px = o->pos.x; e.py = o->pos.y; e.pz = o->pos.z;
        e.es2d = UI2D_EsElemento2D(o);
        e.esTexto = (o->getType() == ObjectType::texto2d);
        e.rot2d = 0; e.ancho = 0; e.alto = 0; e.scrollX = 0; e.scrollY = 0; e.opacidad = 1;
        if (e.es2d) {
            Elemento2D* el = (Elemento2D*)o;
            e.rot2d = el->rot2d; e.ancho = el->ancho; e.alto = el->alto;
            e.scrollX = el->scrollX; e.scrollY = el->scrollY; e.opacidad = el->opacidad;
        } else if (o->getType() == ObjectType::ui) {
            e.ancho = ((UI*)o)->ancho; e.alto = ((UI*)o)->alto;
        }
        if (e.esTexto) e.texto = ((Texto2D*)o)->texto;
        snap->ents.push_back(e);
    }
    if (scripted && o->scriptDatos && !o->scriptDatos->scripts.empty())
        scripted->push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        Recorrer(o->Childrens[i], snap, scripted);
}

static void Snapshot(SimSnap* s) {
    s->ents.clear();
    Recorrer(SceneCollection, s, NULL);
}
static void Aplicar(const SimSnap& s) {
    for (size_t i = 0; i < s.ents.size(); i++) {
        const SimEnt& e = s.ents[i];
        Object* o = e.o; if (!o) continue;
        o->pos.x = e.px; o->pos.y = e.py; o->pos.z = e.pz;
        if (e.es2d) {
            Elemento2D* el = (Elemento2D*)o;
            el->rot2d = e.rot2d; el->ancho = e.ancho; el->alto = e.alto;
            el->scrollX = e.scrollX; el->scrollY = e.scrollY; el->opacidad = e.opacidad;
        } else if (o->getType() == ObjectType::ui) {
            ((UI*)o)->ancho = e.ancho; ((UI*)o)->alto = e.alto;
        }
        if (e.esTexto) ((Texto2D*)o)->texto = e.texto;
    }
    g_redraw = true;
}

bool SimHayScripts() {
    std::vector<Object*> s;
    Recorrer(SceneCollection, NULL, &s);
    return !s.empty();
}

// buscar un objeto por NOMBRE en el arbol (para resolver las referencias)
static Object* BuscarPorNombre(Object* o, const std::string& n) {
    if (!o) return NULL;
    if (o->name == n) return o;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* r = BuscarPorNombre(o->Childrens[i], n);
        if (r) return r;
    }
    return NULL;
}

static void SimPlay() {
    gScripted.clear();
    Recorrer(SceneCollection, NULL, &gScripted);   // en el orden del arbol (outliner)
    if (gScripted.empty()) return;
    Snapshot(&gBase);
    for (size_t i = 0; i < gScripted.size(); i++) {
        Object* s = gScripted[i];
        if (!W3dScriptCargar(s)) continue;
        SimReresolver(s);      // referencias (por nombre) y opciones, de TODOS sus scripts
        W3dScriptInicio(s);
    }
    gGrab.clear();
    gGrabOffset = 0;
    Snapshot(&gBase);          // el inicio() pudo acomodar cosas: ESE es el frame 0
    gGrab.push_back(gBase);
    gTick = 0;
    gActiva = true;
    CurrentFrame = StartFrame;
}

// re-resuelve TODO lo asignado en el editor para los scripts de 'o' (tambien se llama
// al editar una propiedad con el juego andando: el cambio se ve al instante)
void SimReresolver(Object* o) {
    if (!o || !o->scriptDatos) return;
    for (size_t e = 0; e < o->scriptDatos->scripts.size(); e++) {
        const W3dScriptEntrada& ent = o->scriptDatos->scripts[e];
        for (size_t r = 0; r < ent.refs.size(); r++) {
            const std::string& prop = ent.refs[r].first;
            const std::string& val  = ent.refs[r].second;
            Object* obj = BuscarPorNombre(SceneCollection, val);
            if (obj) W3dScriptResolverRef(o, (int)e, prop, obj);
            W3dScriptResolverOpcion(o, (int)e, prop, val);
        }
    }
}

static void TickReal(float dt) {
    // el GAMEPAD analogico entra a los scripts (los ejes ya vienen con deadzone)
    W3dScriptStick(0, axisState[SDL_CONTROLLER_AXIS_LEFTX],  axisState[SDL_CONTROLLER_AXIS_LEFTY]);
    W3dScriptStick(1, axisState[SDL_CONTROLLER_AXIS_RIGHTX], axisState[SDL_CONTROLLER_AXIS_RIGHTY]);
    W3dScriptBotonPad("a", buttonState[SDL_CONTROLLER_BUTTON_A]);
    W3dScriptBotonPad("b", buttonState[SDL_CONTROLLER_BUTTON_B]);
    W3dScriptBotonPad("x", buttonState[SDL_CONTROLLER_BUTTON_X]);
    W3dScriptBotonPad("y", buttonState[SDL_CONTROLLER_BUTTON_Y]);
    for (size_t i = 0; i < gScripted.size(); i++) {
        // un objeto INVISIBLE no ejecuta sus scripts (el checkbox Visible del
        // Control apaga la logica EN VIVO, sin borrarla)
        if (!gScripted[i]->visible) continue;
        W3dScriptActualizar(gScripted[i], dt);
    }
    gTick++;
    { SimSnap s; Snapshot(&s); gGrab.push_back(s); }
    // el cache RUEDA: al llenarse se descartan los frames mas viejos (limite de memoria)
    int techo = (gSimCacheMax > 10) ? gSimCacheMax : 10;
    while ((int)gGrab.size() > techo) { gGrab.erase(gGrab.begin()); gGrabOffset++; }
    if (AnimEsJuego || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
    g_redraw = true;
}

void SimTickPlay(float dt) {
    if (!gActiva) { SimPlay(); if (!gActiva) return; }
    if (gTick + 1 - gGrabOffset < (int)gGrab.size()) {
        // hay FUTURO grabado. "No reemplazar estados": se REPRODUCE lo grabado
        // (sin correr los scripts) hasta alcanzar el borde; sin la opcion, el
        // futuro se descarta y se juega de nuevo desde este momento.
        if (AnimConservarEstados) {
            gTick++;
            Aplicar(gGrab[gTick - gGrabOffset]);
            if (AnimEsJuego || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
            return;
        }
        gGrab.resize(gTick + 1 - gGrabOffset);
    }
    TickReal(dt);
}

// agregar/quitar/cambiar un script CON el juego andando: sus instancias se recargan
// ya mismo (las variables internas de ESOS scripts arrancan de cero)
void SimScriptsCambiados(Object* o) {
    if (!gActiva || !o) return;
    W3dScriptDescargarDe(o);
    bool tiene = (o->scriptDatos && !o->scriptDatos->scripts.empty());
    if (tiene) {
        W3dScriptCargar(o);
        SimReresolver(o);
        W3dScriptInicio(o);
    }
    // si es su PRIMER script en vivo, entra a la lista de ejecucion
    bool enLista = false;
    for (size_t i = 0; i < gScripted.size(); i++) if (gScripted[i] == o) enLista = true;
    if (tiene && !enLista) gScripted.push_back(o);
}

void SimIrA(int tick) {
    if (!gActiva || gGrab.empty()) return;
    if (tick < gGrabOffset) tick = gGrabOffset;
    if (tick > gGrabOffset + (int)gGrab.size() - 1) tick = gGrabOffset + (int)gGrab.size() - 1;
    gTick = tick;
    Aplicar(gGrab[gTick - gGrabOffset]);
    if (AnimEsJuego || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
}

bool SimStep(int dir) {
    if (!gActiva) return false;
    if (dir < 0) {
        if (gTick <= gGrabOffset) return true;   // el cache rodo: mas atras no hay
        gTick--;
        Aplicar(gGrab[gTick - gGrabOffset]);     // VIAJE EN EL TIEMPO: re-ver el anterior
        if (AnimEsJuego || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
    } else {
        if (gTick + 1 - gGrabOffset < (int)gGrab.size()) {
            gTick++;
            Aplicar(gGrab[gTick - gGrabOffset]);     // re-ver lo ya grabado
            if (AnimEsJuego || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
        } else {
            TickReal(1.0f / 30.0f);    // borde de lo grabado: simular un frame nuevo
        }
    }
    return true;
}

void SimStop() {
    if (!gActiva) return;
    Aplicar(gBase);
    W3dScriptDescargarTodo();
    W3dScriptSoltarTeclas();
    gGrab.clear();
    gScripted.clear();
    gTick = 0;
    gGrabOffset = 0;
    gActiva = false;
    CurrentFrame = StartFrame;
    g_redraw = true;
}

// ---- teclado del juego -----------------------------------------------------
void SimTeclaSDL(int sdlk, bool down) {
    const char* n = NULL;
    char letra[2] = { 0, 0 };
    if (sdlk >= SDLK_a && sdlk <= SDLK_z) { letra[0] = (char)('a' + (sdlk - SDLK_a)); n = letra; }
    else if (sdlk == SDLK_UP)     n = "arriba";
    else if (sdlk == SDLK_DOWN)   n = "abajo";
    else if (sdlk == SDLK_LEFT)   n = "izquierda";
    else if (sdlk == SDLK_RIGHT)  n = "derecha";
    else if (sdlk == SDLK_SPACE)  n = "espacio";
    else if (sdlk >= SDLK_0 && sdlk <= SDLK_9) { letra[0] = (char)('0' + (sdlk - SDLK_0)); n = letra; }
    if (n) W3dScriptTecla(n, down);
}
