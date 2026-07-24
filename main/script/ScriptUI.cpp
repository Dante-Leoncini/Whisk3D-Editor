// ============================================================================
//  ScriptUI — el binding 2D que ven los scripts lua EN EL EDITOR (y en el
//  juego de PC): trabaja en PIXELES del lienzo con (0,0) en el CENTRO (los
//  elementos del pong usan ancla centro). El Core solo aporta el interprete;
//  esta API conoce los elementos 2D (UI/Texto2D/Elemento2D) que son del
//  editor. Se registra via W3dScriptSetBindExtra.
//
//    pantalla()        -> ancho, alto del lienzo (px)
//    posPx(o)          -> x, y (px, centro = 0,0)
//    setPosPx(o, x, y)
//    tamPx(o)          -> ancho, alto (px)
//    setTamPx(o, w, h)
//    setTexto(o, v)    -> el texto de un elemento Texto (numeros ok)
// ============================================================================
#include "script/W3dScript.h"
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Elemento2D.h"
#include "objects/Texto2D.h"
#include "render/UIOverlay.h"
#include "render/OpcionesRender.h"   // g_redraw
#include "objects/Camera.h"          // CameraActive (movimiento relativo a la camara)
#include "objects/Gamepad.h"         // el objeto Script/gamepad (objetivo() = su target)

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <stdio.h>

// el lienzo del juego: el UI del editor 2D si hay, sino el primero de la escena
static UI* UIJuego() {
    UI* u = UI2D_UIDelEditor();
    if (u) return u;
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            if (SceneCollection->Childrens[i]->getType() == ObjectType::ui)
                return (UI*)SceneCollection->Childrens[i];
    return NULL;
}
static void TamLienzo(float* w, float* h) {
    UI* u = UIJuego();
    *w = u ? u->ancho : 1.0f;
    *h = u ? u->alto : 1.0f;
}

static int LPantalla(lua_State* L) {
    float w, h; TamLienzo(&w, &h);
    lua_pushnumber(L, w); lua_pushnumber(L, h);
    return 2;
}
static int LPosPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (!o) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    lua_pushnumber(L, o->pos.x * w);
    lua_pushnumber(L, o->pos.y * h);
    return 2;
}
static int LSetPosPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (o && w > 0.0f && h > 0.0f) {
        o->pos.x = (float)luaL_checknumber(L, 2) / w;
        o->pos.y = (float)luaL_checknumber(L, 3) / h;
        g_redraw = true;
    }
    return 0;
}
static int LTamPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (o && UI2D_EsElemento2D(o)) {
        Elemento2D* e = (Elemento2D*)o;
        lua_pushnumber(L, e->tamPx ? e->ancho : e->ancho * w);
        lua_pushnumber(L, e->tamPx ? e->alto  : e->alto  * h);
    } else { lua_pushnumber(L, 0); lua_pushnumber(L, 0); }
    return 2;
}
static int LSetTamPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (o && UI2D_EsElemento2D(o) && w > 0.0f && h > 0.0f) {
        Elemento2D* e = (Elemento2D*)o;
        float nw = (float)luaL_checknumber(L, 2), nh = (float)luaL_checknumber(L, 3);
        if (e->tamPx) { e->ancho = nw; e->alto = nh; }
        else          { e->ancho = nw / w; e->alto = nh / h; }
        g_redraw = true;
    }
    return 0;
}
static int LSetTexto(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (o && o->getType() == ObjectType::texto2d) {
        const char* s = lua_tostring(L, 2);   // numeros se convierten solos
        ((Texto2D*)o)->texto = s ? s : "";
        g_redraw = true;
    }
    return 0;
}

// camaraXZ() -> fx, fz, rx, rz: el adelante y la derecha de la CAMARA proyectados al
// piso (para mover un personaje relativo a como estas mirando)
static int LCamaraXZ(lua_State* L) {
    float fx = 0, fz = -1, rx = 1, rz = 0;
    if (CameraActive) {
        Vector3 f = CameraActive->forwardVector, r = CameraActive->rightVector;
        Vector3 fXZ = Vector3(f.x, 0.0f, f.z).Normalized();
        Vector3 rXZ = Vector3(r.x, 0.0f, r.z).Normalized();
        fx = fXZ.x; fz = fXZ.z; rx = rXZ.x; rz = rXZ.z;
    }
    lua_pushnumber(L, fx); lua_pushnumber(L, fz);
    lua_pushnumber(L, rx); lua_pushnumber(L, rz);
    return 4;
}
// objetivo() -> el TARGET del objeto dueno del script (el personaje que maneja un
// objeto Script/gamepad); nil si no tiene
static int LObjetivo(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_duenio");
    Object* duenio = (Object*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (duenio && duenio->getType() == ObjectType::gamepad) {
        Object* t = ((Gamepad*)duenio)->target;
        if (t) { lua_pushlightuserdata(L, t); return 1; }
    }
    lua_pushnil(L);
    return 1;
}

// parametro("piso") -> los valores del objeto Script/gamepad DEL PROYECTO (los que
// el .w3d importo): asi la fisica del lua respeta lo configurado en cada juego
static int LParametro(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    float def = (float)luaL_optnumber(L, 2, 0.0);
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_duenio");
    Object* duenio = (Object*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    float v = def;
    if (duenio && duenio->getType() == ObjectType::gamepad) {
        Gamepad* g = (Gamepad*)duenio;
        std::string k = n;
        if      (k == "velocidad")            v = g->velocidad;
        else if (k == "piso")                 v = g->piso;
        else if (k == "gravedad")             v = g->gravedad;
        else if (k == "limiteIzquierdo")      v = g->limiteIzquierdo;
        else if (k == "limiteDerecho")        v = g->limiteDerecho;
        else if (k == "limiteFondo")          v = g->limiteFondo;
        else if (k == "limiteFrente")         v = g->limiteFrente;
        else if (k == "velocidadMaximaCaida") v = g->velocidadMaximaCaida;
        else if (k == "potenciaSalto")        v = g->potenciaSalto;
    }
    lua_pushnumber(L, v);
    return 1;
}

static void RegistrarBindUI(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    lua_pushcfunction(L, LPantalla); lua_setglobal(L, "pantalla");
    lua_pushcfunction(L, LPosPx);    lua_setglobal(L, "posPx");
    lua_pushcfunction(L, LSetPosPx); lua_setglobal(L, "setPosPx");
    lua_pushcfunction(L, LTamPx);    lua_setglobal(L, "tamPx");
    lua_pushcfunction(L, LSetTamPx); lua_setglobal(L, "setTamPx");
    lua_pushcfunction(L, LSetTexto); lua_setglobal(L, "setTexto");
    lua_pushcfunction(L, LCamaraXZ); lua_setglobal(L, "camaraXZ");
    lua_pushcfunction(L, LObjetivo); lua_setglobal(L, "objetivo");
    lua_pushcfunction(L, LParametro); lua_setglobal(L, "parametro");
}

// registrar al arrancar (constructor estatico simple: corre antes de main)
namespace { struct RegistroBind { RegistroBind() { W3dScriptSetBindExtra(RegistrarBindUI); } }; }
static RegistroBind gRegistroBind;
