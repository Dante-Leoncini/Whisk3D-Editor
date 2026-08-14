// ============================================================================
//  ScriptUI — el punto de registro de los binds lua del Play. TODOS los binds de
//  JUEGO son COMPARTIDOS con el runtime compilado: viven en BindsJuego.{h,cpp} y
//  se registran igual en los dos lados, para que un juego corra identico en Play
//  y compilado. Aca queda el enganche (W3dScriptSetBindExtra) y el hook de
//  redibujo, que si es del editor.
// ============================================================================
#include "script/W3dScript.h"
#include "script/BindsJuego.h"        // BindsJuegoRegistrar: TODO el set de binds de juego
#include "objects/Objects.h"
#include "render/OpcionesRender.h"   // g_redraw / g_objetosMovidos (hook de redibujo de los binds del Core)

static void RegistrarBindUI(void* Lv) {
    // TODO el set de binds de juego es COMPARTIDO con el runtime compilado y vive
    // en BindsJuego.cpp. camaraXZ/objetivo/parametro se registraban ACA, o sea SOLO
    // en el Play: el mismo .lua tenia tres funciones menos adentro del .deb y el
    // juego compilado reventaba al llamarlas ("attempt to call a nil value"). Se
    // mudaron al set compartido (los dos que tocan objetos, por hook). Ya no queda
    // ningun bind de juego propio del editor.
    BindsJuegoRegistrar(Lv);
}

// HOOK de redibujo para los binds de OBJETO del Core (setPosicion/mover/girar/setEscala/...): el
// Core no ve estas variables (son de main/), asi que le pasamos una funcion que las prende. Sin esto
// un cambio hecho desde lua no repinta (el editor es event-driven) y el modificador Espejo con
// target no se regenera.
static void RedibujarPorScript() {
    g_redraw = true;
    g_objetosMovidos = true;
}

// registrar al arrancar (constructor estatico simple: corre antes de main)
namespace { struct RegistroBind { RegistroBind() {
    W3dScriptSetBindExtra(RegistrarBindUI);
    W3dScriptSetRedibujo(RedibujarPorScript);
} }; }
static RegistroBind gRegistroBind;
