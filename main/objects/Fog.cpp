// ============================================================================
//  Fog.cpp — ver Fog.h
// ============================================================================
#include "objects/Fog.h"
#include "graphics/w3dGraphics.h"
#include "render/RenderOptions.h"   // g_renderBg (fondo del render)
#include "script/BindsGame.h"        // W3dFogHook: fog()/setNiebla() de lua

namespace gfx = w3dEngine;

Fog::Fog(Object* parent, Vector3 pos)
    : Object(parent, "Fog", pos), active(true), mode(0), start(5.0f), end(60.0f), density(0.02f), background(false) {
    color[0] = 0.55f; color[1] = 0.53f; color[2] = 0.46f; color[3] = 1.0f;
}

void Fog::RenderObject() {
    // solo en Render (w3dRenderLights = el pase Rendered del editor y el del juego)
    if (!w3dRenderLights) return;
    if (!active) { gfx::Disable(gfx::Fog); return; }
    gfx::FogMode(mode);
    gfx::FogStart(start);
    gfx::FogEnd(end > start + 0.001f ? end : start + 0.001f);
    gfx::FogDensity(density);
    float c[4] = { color[0], color[1], color[2], 1.0f };
    gfx::FogColor(c);
    gfx::Enable(gfx::Fog);
    if (background) { g_renderBg[0] = color[0]; g_renderBg[1] = color[1]; g_renderBg[2] = color[2]; }
}

// fog()/setNiebla() de lua (BindsGame.cpp)
static bool FogHook(Object* o, bool write, float* v, int n) {
    if (!o || o->getType() != ObjectType::fog || n < 6) return false;
    Fog* f = (Fog*)o;
    if (write) {
        f->density = v[0] < 0.0f ? 0.0f : v[0];
        for (int k = 0; k < 3; k++) f->color[k] = v[1 + k] < 0.0f ? 0.0f : (v[1 + k] > 1.0f ? 1.0f : v[1 + k]);
        f->start = v[4]; f->end = v[5];
    } else {
        v[0] = f->density; v[1] = f->color[0]; v[2] = f->color[1]; v[3] = f->color[2]; v[4] = f->start; v[5] = f->end;
    }
    return true;
}
struct FogHookReg { FogHookReg() { W3dFogHook = FogHook; } };
static FogHookReg gFogHookReg;

void W3dFogRestart() {
    if (w3dRenderLights) gfx::Disable(gfx::Fog);
}
