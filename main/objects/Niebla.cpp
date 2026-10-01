// ============================================================================
//  Niebla.cpp — ver Niebla.h
// ============================================================================
#include "objects/Niebla.h"
#include "gfx/w3dGraphics.h"
#include "render/OpcionesRender.h"   // g_renderBg (fondo del render)
#include "script/BindsJuego.h"        // W3dNieblaHook: niebla()/setNiebla() de lua

namespace gfx = w3dEngine;

Niebla::Niebla(Object* parent, Vector3 pos)
    : Object(parent, "Niebla", pos), activa(true), modo(0), inicio(5.0f), fin(60.0f), densidad(0.02f), fondo(false) {
    color[0] = 0.55f; color[1] = 0.53f; color[2] = 0.46f; color[3] = 1.0f;
}

void Niebla::RenderObject() {
    // solo en Render (w3dRenderLuces = el pase Rendered del editor y el del juego)
    if (!w3dRenderLuces) return;
    if (!activa) { gfx::Disable(gfx::Fog); return; }
    gfx::FogModo(modo);
    gfx::FogStart(inicio);
    gfx::FogEnd(fin > inicio + 0.001f ? fin : inicio + 0.001f);
    gfx::FogDensidad(densidad);
    float c[4] = { color[0], color[1], color[2], 1.0f };
    gfx::FogColor(c);
    gfx::Enable(gfx::Fog);
    if (fondo) { g_renderBg[0] = color[0]; g_renderBg[1] = color[1]; g_renderBg[2] = color[2]; }
}

// niebla()/setNiebla() de lua (BindsJuego.cpp)
static bool NieblaHook(Object* o, bool escribir, float* v, int n) {
    if (!o || o->getType() != ObjectType::niebla || n < 6) return false;
    Niebla* f = (Niebla*)o;
    if (escribir) {
        f->densidad = v[0] < 0.0f ? 0.0f : v[0];
        for (int k = 0; k < 3; k++) f->color[k] = v[1 + k] < 0.0f ? 0.0f : (v[1 + k] > 1.0f ? 1.0f : v[1 + k]);
        f->inicio = v[4]; f->fin = v[5];
    } else {
        v[0] = f->densidad; v[1] = f->color[0]; v[2] = f->color[1]; v[3] = f->color[2]; v[4] = f->inicio; v[5] = f->fin;
    }
    return true;
}
struct NieblaHookReg { NieblaHookReg() { W3dNieblaHook = NieblaHook; } };
static NieblaHookReg gNieblaHookReg;

void W3dNieblaReiniciar() {
    if (w3dRenderLuces) gfx::Disable(gfx::Fog);
}
