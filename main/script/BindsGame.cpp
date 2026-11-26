// ============================================================================
//  BindsGame.cpp — ver BindsGame.h. Los binds 2D de juego COMPARTIDOS por el
//  editor (Play) y el runtime compilado, mas el estado por-frame que necesitan.
//  C++03 puro (Symbian/Android). Comentarios sin acentos.
// ============================================================================
#include "objects/Routine.h"
#include "objects/MeshFlows.h"    // step(): los nombres de las primitivas
#include <time.h>                 // clockMs sin el perfilador: clock()
#include <ctype.h>   // setMemory / memoria: las memorias que leen las rutinas
#include "script/BindsGame.h"
#include "script/W3dScript.h"          // W3dScriptParamObject
#include "animation/Animation.h"       // g_activeCameraHook (setActiveCamera)
#include "physics/W3dPhysics.h"        // adaptador 2D de la fisica del Core (lienzo + tamano del elemento)
#include "W3dScene.h"                  // changeScene(): el bind multi-escena COMPARTIDO
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Text2D.h"
#include "objects/Button2D.h"
#include "objects/Image2D.h"
#include "objects/Rect2D.h"             // setTint: el relleno de un rect 2D
#include "objects/Mesh.h"               // setTint: el tinte de una malla 3D (Mesh::tinte)
#include "objects/Element2D.h"
#include "render/UIOverlay.h"         // UI2D_CanvasSize, UI2D_IsElement2D, UI2DPos
#include "render/RenderOptions.h"     // g_redraw (+ g_renderAspect para screenOf)
#include "base/w3dlog.h"              // w3dLogf: log PROPIO (nunca printf/stdout -> abria la consola blanca)
// (Camera.h NO se incluye aca: arrastra variables.h, que es solo del editor. Todo
//  lo que necesita una camara pasa por los hooks de BindsGame.h, que instala quien
//  linkee el objeto Camera. Asi el bind existe y funciona en cualquier build que
//  traiga camara, en vez de ser un stub mudo segun el #ifdef del build.)
#include "io/UI2DFormat.h"             // g_w3dDirProject: base de las rutas relativas de sound()
#include "W3dStore.h"                  // W3dMountedStore(): setTexture respeta las entradas del contenedor v4
#include "objects/Materials.h"         // setTextureB / setTextureMix: la mezcla de texturas de un material
#include "objects/Textures.h"
#include "render/SceneRender.h"     // captureScreen / captureUV
#include "base/W3dConfig.h"            // ConfigMute(): mute global del Core
#include "W3dNotice.h"                 // W3dAvisof: el aviso de control conectado/desconectado
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <math.h>   // fabsf() para collide()
#include <map>      // cache de sonidos cargados por sound()
#include <algorithm> // luaProfile: lo mas caro primero
#include "io/w3dFilesystem.h"   // ListDir: la precarga de WAVs recorre sounds/ y music/
#include <string>

// audio del Core, forward-decl (evita el include del dir de audio, como en W3dScript.cpp).
// Sin -DW3D_ENABLE_AUDIO son stubs no-op -> sound() no hace nada y no rompe.
#include "audio/W3dAudio.h"   // el mixer: sound(), la envolvente y la reverb

// ---------------------------------------------------------------------------
//  TAMANO del lienzo del juego: el MISMO que ve el render del overlay
//  (UI2D_CanvasSize). En el editor es el UI que se edita; en el runtime es
//  g_gameRenderW/H (que W3dGameRender deja igual al lienzo). Asi el script
//  (screen/getPosPx) y el dibujo COINCIDEN en los dos lados.
// ---------------------------------------------------------------------------
static void CanvasSize(float* w, float* h) {
    UI2D_CanvasSize(w, h);
    if (*w < 1.0f) *w = 1.0f;
    if (*h < 1.0f) *h = 1.0f;
}

// ---------------------------------------------------------------------------
//  ESTADO por-frame (lo alimentan los dos builds; ver BindsGame.h).
// ---------------------------------------------------------------------------
// mapeo del ultimo render (ventana -> lienzo): escala + offset del letterbox / zona segura.
// OJO: solo valido DESPUES del primer render (arranca en 1,0,0) -> no en start().
static float gLastEsc = 1.0f, gLastX0 = 0.0f, gLastY0 = 0.0f;
// rect SEGURO (sin notch/barras) en px de VENTANA, tal como lo uso el ultimo render.
static float gSafeWinX = 0.0f, gSafeWinY = 0.0f, gSafeWinW = 0.0f, gSafeWinH = 0.0f;
// posiciones RESUELTAS por el layout (anclas/flex) en el ULTIMO render: para que
// uiBox() lea el rect ya resuelto de un objeto anclado sin recalcularlo.
static std::vector<UI2DPos> gLastPos;

// PUNTEROS en coords de LIENZO (centro 0,0), las MISMAS que getPosPx/getSizePx, mas el estado
// apretado ACTUAL y el del frame ANTERIOR (para el flanco de bajada de pressed()). Los
// slots son los dedos 0..3; el clic del mouse entra por el slot 0 (dedo 0 / tap()).
#define W3D_GAME_MAXDEDOS 4
static float gPunX[W3D_GAME_MAXDEDOS]    = {0,0,0,0};
static float gPunY[W3D_GAME_MAXDEDOS]    = {0,0,0,0};
static bool  gPunAct[W3D_GAME_MAXDEDOS]  = {false,false,false,false};
static bool  gPunPrev[W3D_GAME_MAXDEDOS] = {false,false,false,false};

// MOUSE-OVER en coords de LIENZO (centro 0,0): solo lo alimentan los eventos de MOUSE
// (runtime W3dGameMouse / editor SimMouseScreen). El tactil NO pasa por aca, asi que
// gMousePresent false = sin mouse = sin hover (el borde de hover no se dibuja).
static float gMouseCX = 0.0f, gMouseCY = 0.0f;
static bool  gMousePresent = false;

// pedido de cierre del juego (lua quit()).
static bool gQuit = false;

// ---------------------------------------------------------------------------
//  Helpers de rect.
// ---------------------------------------------------------------------------
// tamano DECLARADO del elemento en px de LIENZO segun su unidad (sizeMode): fraccion
// del lienzo, px directos, o px ESCALADOS (valor * min(w,h)/480, el factor del bind
// getScale()). El MISMO tamano que resuelve el dibujo (render/UIOverlay.cpp).
static void DeclaredSize(Element2D* e, float w, float h, float* aw, float* ah) {
    if (e->sizeMode == TAM2D_PX) { *aw = e->width; *ah = e->height; return; }
    if (e->sizeMode == TAM2D_SCALED) {
        float m = (w < h) ? w : h;
        float f = (m >= 1.0f) ? m / 480.0f : 1.0f;
        *aw = e->width * f; *ah = e->height * f;
        return;
    }
    *aw = e->width * w; *ah = e->height * h;
}
// rect del objeto en coords de LIENZO (centro 0,0): centro (cx,cy) + medias extensiones
// (hw,hh). Respeta la unidad del tamano (sizeMode). false si el objeto es nil.
static bool RectCanvas(Object* o, float* cx, float* cy, float* hw, float* hh) {
    if (!o) return false;
    float w, h; CanvasSize(&w, &h);
    *cx = o->pos.x * w; *cy = o->pos.y * h;
    float aw = 0.0f, ah = 0.0f;
    if (UI2D_IsElement2D(o)) DeclaredSize((Element2D*)o, w, h, &aw, &ah);
    *hw = aw * 0.5f; *hh = ah * 0.5f;
    return true;
}
// busca el rect que OCUPO el objeto 'o' en el ULTIMO render y lo pasa a coords de LIENZO
// (centro 0,0), con (cx,cy) = CENTRO y (aw,ah) = tamano. false si no se dibujo.
static bool ResolvedRectCanvas(Object* o, float* cx, float* cy, float* aw, float* ah) {
    if (!o || gLastEsc <= 0.0001f) return false;
    float lw, lh; CanvasSize(&lw, &lh);
    for (size_t i = 0; i < gLastPos.size(); i++) {
        if (gLastPos[i].obj != o) continue;
        const UI2DPos& p = gLastPos[i];
        float mx = (p.bx0 + p.bx1) * 0.5f, my = (p.by0 + p.by1) * 0.5f;   // centro en px de ventana
        *cx = (mx - gLastX0) / gLastEsc - lw * 0.5f;   // ventana -> lienzo
        *cy = (my - gLastY0) / gLastEsc - lh * 0.5f;
        *aw = (p.bx1 - p.bx0) / gLastEsc;
        *ah = (p.by1 - p.by0) / gLastEsc;
        return true;
    }
    return false;
}
// CAJA de un objeto en coords de LIENZO (centro + tamano): el rect RESUELTO por el layout si ya se
// dibujo, y si no la pos/tam crudos. Es lo que devuelve uiBox() y lo que usan limit(o, area),
// inside(area, x, y) y la fisica cuando un objeto hace de area: un solo lugar, una sola respuesta.
// El punto de su ancla (borde o esquina del lienzo) en coords de LIENZO, para un elemento colgado de la raiz UI en layout
// libre y sin padding: ahi su pos es un corrimiento desde ese punto (render/UIOverlay.cpp). En cualquier otro caso 0, 0
// (la pos cruda es lo unico que se sabe).
static void RawAnchorCanvas(Object* o, float w, float h, float* ax, float* ay) {
    *ax = *ay = 0.0f;
    if (!o || !UI2D_IsElement2D(o) || !o->Parent || o->Parent->getType() != ObjectType::ui) return;
    UI* u = (UI*)o->Parent;
    if (u->layoutChildren != 0 || u->padLeft != 0.0f || u->padRight != 0.0f || u->padArr != 0.0f || u->padBottom != 0.0f) return;
    UI2D_PointAnchor(((Element2D*)o)->anchor, -w * 0.5f, -h * 0.5f, w, h, ax, ay);
}
static bool BoxCanvas(Object* o, float* cx, float* cy, float* aw, float* ah) {
    if (!o) return false;
    if (ResolvedRectCanvas(o, cx, cy, aw, ah)) return true;
    float w, h; CanvasSize(&w, &h);
    // (sin el rect del ultimo render -el HUD dibujado sobre el 3D del juego no lo pasa-: el de un elemento anclado a un
    //  borde o esquina sale de su ancla; antes era la pos sola y el toque de un boton del HUD en la esquina no le pegaba)
    float ax, ay;
    RawAnchorCanvas(o, w, h, &ax, &ay);
    *cx = ax + o->pos.x * w; *cy = ay + o->pos.y * h; *aw = 0.0f; *ah = 0.0f;
    if (UI2D_IsElement2D(o)) DeclaredSize((Element2D*)o, w, h, aw, ah);
    return true;
}
// ---------------------------------------------------------------------------
//  REGLA CRUDO-vs-RESUELTO (colisiones: collide/limit y la fisica del Core).
//  Hay dos "verdades" posibles para la posicion de un objeto 2D:
//    * la CRUDA (o->pos * lienzo): es del FRAME ACTUAL, pero solo coincide con
//      lo dibujado si el layout no lo mueve;
//    * la RESUELTA (gLastPos, el rect del ULTIMO render): siempre coincide con
//      lo dibujado, pero tiene 1 frame de atraso.
//  Para los objetos de GAMEPLAY que el lua/fisica MUEVEN cada frame (pelota,
//  palas: hijos directos de la raiz, ancla centro, layout libre) la verdad es
//  la CRUDA: el render de ESTE frame los va a dibujar ahi, y usar la resuelta
//  les meteria un frame de lag en las colisiones (regresion del pong). Para
//  los objetos PUESTOS POR EL LAYOUT (dentro de un flex, o anclados a un
//  borde/esquina) la cruda es basura y la verdad es la RESUELTA.
// ---------------------------------------------------------------------------
// true si la pos CRUDA del objeto es AUTORITATIVA (coincide con donde el render
// de ESTE frame lo va a dibujar). Las condiciones replican el dibujo
// (render/UIOverlay.cpp, DrawElemRec):
//   * Element2D hijo DIRECTO de la raiz UI: en los anidados la pos se guarda
//     RELATIVA al rect del padre (pos.x * anchoDelPadre), asi que pos * lienzo
//     no es su centro;
//   * la raiz en layout LIBRE (layoutChildren == 0): en filas/columnas manda la
//     celda y la pos se ignora;
//   * ancla CENTRO (ancla == 0): con ancla a borde/esquina la pos es un
//     corrimiento desde ese punto, no desde el centro del lienzo;
//   * la raiz SIN padding: el padding corre el rect de referencia del ancla.
static bool RawPosAuthoritative(Object* o) {
    if (!o || !UI2D_IsElement2D(o)) return false;
    Object* p = o->Parent;
    if (!p || p->getType() != ObjectType::ui) return false;
    UI* u = (UI*)p;
    if (u->layoutChildren != 0) return false;
    if (((Element2D*)o)->anchor != 0) return false;
    if (u->padLeft != 0.0f || u->padRight != 0.0f ||
        u->padArr != 0.0f || u->padBottom != 0.0f) return false;
    return true;
}
// CAJA DE COLISION de un operando de collide()/limit(): centro (cx,cy) +
// MEDIAS extensiones (hw,hh) aplicando la regla crudo-vs-resuelto:
//   * pos cruda AUTORITATIVA -> centro CRUDO del frame actual + tamano
//     DECLARADO (DeclaredSize respeta sizeMode): sin lag, como siempre;
//   * si no -> BoxCanvas (rect RESUELTO del ultimo render, que es donde el
//     objeto se dibuja de verdad; con fallback al crudo si nunca se dibujo).
static bool BoxCollision(Object* o, float* cx, float* cy, float* hw, float* hh) {
    if (!o) return false;
    if (RawPosAuthoritative(o)) return RectCanvas(o, cx, cy, hw, hh);
    float aw = 0.0f, ah = 0.0f;
    if (!BoxCanvas(o, cx, cy, &aw, &ah)) return false;
    *hw = aw * 0.5f; *hh = ah * 0.5f;
    return true;
}
// alto (en px de LIENZO) del rect de REFERENCIA del ancla del objeto en el
// ultimo render: la unidad en la que se guarda su pos.y (pos.y = fraccion de
// este alto). Lo usa limit() para aplicar un delta a un objeto NO
// autoritativo sin teletransportarlo. false si el objeto no se dibujo.
static bool RefCanvasHeight(Object* o, float* refH) {
    if (!o || gLastEsc <= 0.0001f) return false;
    for (size_t i = 0; i < gLastPos.size(); i++) {
        if (gLastPos[i].obj != o) continue;
        if (gLastPos[i].refH <= 0.0001f) return false;
        *refH = gLastPos[i].refH / gLastEsc;   // px de ventana -> px de lienzo
        return true;
    }
    return false;
}
// ANCHO del rect de referencia, espejo exacto del de arriba: la unidad en la que
// se guarda pos.x. Lo necesita limit() para poder recortar tambien en X.
static bool RefCanvasWidth(Object* o, float* refW) {
    if (!o || gLastEsc <= 0.0001f) return false;
    for (size_t i = 0; i < gLastPos.size(); i++) {
        if (gLastPos[i].obj != o) continue;
        if (gLastPos[i].refW <= 0.0001f) return false;
        *refW = gLastPos[i].refW / gLastEsc;   // px de ventana -> px de lienzo
        return true;
    }
    return false;
}
// CAJA para el HIT-TEST de TOQUE (apretado): el rect RESUELTO por el layout (BoxCanvas,
// con su fallback al crudo si el objeto no se dibujo aun), ENSANCHADO al tamano DECLARADO
// del elemento (max por eje, centrado en el centro resuelto). El ensanche es porque para
// texto2d el rect resuelto que guarda el overlay es el BOUNDS medido de los glifos, mas
// chico que la caja ancho/alto del .w3dui: sin esto el area de toque de una opcion de menu
// se achicaria a las letras. Para boton2d/rect el resuelto ya es el rect dibujado y el max
// solo garantiza no tocar MENOS que antes. Devuelve MEDIAS extensiones (hw,hh) como RectCanvas.
static bool BoxHitCanvas(Object* o, float* cx, float* cy, float* hw, float* hh) {
    float aw = 0.0f, ah = 0.0f;
    if (!BoxCanvas(o, cx, cy, &aw, &ah)) return false;
    // ensanche SOLO para texto2d: su rect resuelto es el bounds MEDIDO de los glifos (mas chico
    // que la caja de toque declarada con ancho/alto) y sin esto el area de tap se achicaria.
    // Para button/rect/contenedor el rect resuelto por el layout ES la caja real: usarla tal cual.
    // (Ensanchar con el tamano "declarado" en esos tipos rompia las grillas flex: los botones que
    // no declaran tamano heredan el default del constructor (200x150) y pisaban a los vecinos.)
    if (o->getType() == ObjectType::text2d && UI2D_IsElement2D(o)) {
        float w, h; CanvasSize(&w, &h);
        float dw, dh;   // tamano declarado en px de lienzo (respeta sizeMode)
        DeclaredSize((Element2D*)o, w, h, &dw, &dh);
        if (dw > aw) aw = dw;
        if (dh > ah) ah = dh;
    }
    *hw = aw * 0.5f; *hh = ah * 0.5f;
    return true;
}

// ---------------------------------------------------------------------------
//  BINDS (coordenadas en px de lienzo, centro 0,0).
// ---------------------------------------------------------------------------
// screenMargin() -> mx, my: cuanto se extiende la pantalla MAS ALLA del lienzo del HUD encajado (letterbox), por lado,
// en unidades del lienzo, hasta los bordes REALES (las bandas del encuadre de la camara tambien son pantalla). 0, 0 si
// el HUD llena la pantalla. Un elemento anclado arriba que se quiere pegar al borde de arriba (en vertical, con el
// lienzo 4:3 centrado) se corre 'my' hacia arriba; uno de abajo, hacia abajo
float g_hudMarginX = 0.0f, g_hudMarginY = 0.0f;
static int LMarginScreen(lua_State* L) {
    lua_pushnumber(L, g_hudMarginX); lua_pushnumber(L, g_hudMarginY); return 2;
}
// screenFrame() -> x, y, ancho, alto: el CUADRO DE LA IMAGEN 3D (el encuadre de la camara, sin las bandas) en
// unidades del lienzo, x e y desde el centro del lienzo (y hacia abajo), como getPosPx de un elemento anclado al centro.
// En vertical con la camara 4:3 y el modo cover, la imagen es mas alta que el lienzo; con bandas, es el lienzo
float g_hudFrame[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static int LFrameScreen(lua_State* L) {
    for (int i = 0; i < 4; i++) lua_pushnumber(L, g_hudFrame[i]);
    return 4;
}
// captureScreen(nombre [, withHud]) -> ruta: copia la imagen 3D de ESTE cuadro (al terminar la escena, antes del HUD;
// con 'withHud', despues de dibujar el HUD: lo que ve el jugador) a una textura; la ruta ("pantalla:<nombre>") sirve para setTexture de una imagen 2D, un material o "Bind texture" de una
// rutina. captureUV(nombre) -> u0, v0, u1, v1: la parte de la textura con la imagen derecha (para setUVRect), o nil
// si todavia no se copio (la copia pedida se hace al dibujar: puede pasar mas de un actualizar). Ver SceneRender.h
static int LCaptureScreen(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    W3dCaptureRequest(n, lua_toboolean(L, 2) != 0);
    g_redraw = true;
    lua_pushstring(L, (std::string("screen:") + n).c_str());
    return 1;
}
static int LCaptureUV(lua_State* L) {
    float uv[4];
    if (!W3dCaptureUV(luaL_checkstring(L, 1), uv)) return 0;
    for (int i = 0; i < 4; i++) lua_pushnumber(L, uv[i]);
    return 4;
}
static int LScreen(lua_State* L) {
    float w, h; CanvasSize(&w, &h);
    lua_pushnumber(L, w); lua_pushnumber(L, h); return 2;
}
// resolution(): ancho y alto en PIXELES REALES de la imagen 3D que se dibuja (no el lienzo de la UI). Sirve para
// elegir detalle por tamaño en pantalla. Antes del primer frame devuelve 0, 0.
static int LResolution(lua_State* L) {
    extern float g_renderImageHeightPx; extern float g_renderCamAspect;
    lua_pushnumber(L, g_renderImageHeightPx * g_renderCamAspect); lua_pushnumber(L, g_renderImageHeightPx); return 2;
}
// ---- MEMORIAS de las rutinas (objects/Routine.h): bloques de 256 floats por nombre que leen los pasos "@nombre[i]" ----
// setMemory(nombre, i, v) / getMemory(nombre, i) / setMemories(nombre, i, v1, v2, ...): escribe varias seguidas
static int LSetMemory(lua_State* L) {
    float* m = W3dMemory(luaL_checkstring(L, 1));
    const int i = (int)luaL_checkinteger(L, 2);
    if (i >= 0 && i < W3D_MEMORY_SIZE) m[i] = (float)luaL_checknumber(L, 3);
    return 0;
}
static int LMemory(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    const int i = (int)luaL_checkinteger(L, 2);
    lua_pushnumber(L, (W3dMemoryExists(n) && i >= 0 && i < W3D_MEMORY_SIZE) ? W3dMemory(n)[i] : 0.0f);
    return 1;
}
static int LSetMemories(lua_State* L) {
    float* m = W3dMemory(luaL_checkstring(L, 1));
    int i = (int)luaL_checkinteger(L, 2);
    const int top = lua_gettop(L);
    for (int a = 3; a <= top && i < W3D_MEMORY_SIZE; a++, i++) if (i >= 0) m[i] = (float)lua_tonumber(L, a);
    return 0;
}
// opcion de un paso por su nombre (la etiqueta en ingles, sin importar mayusculas; en "dibujar", tambien el nombre del
// .w3d: "tira", "abanico"...) o por su numero. -1 = no existe
static int OptionLua(int t, const std::string& s) {
    const int l = W3dStepOptions(t);
    std::string b;
    for (size_t i = 0; i < s.size(); i++) b += (char)tolower((unsigned char)s[i]);
    for (int i = 0; i < W3dOptionsN(l); i++) {
        std::string e = W3dOptionLabel(l, i);
        for (size_t k = 0; k < e.size(); k++) e[k] = (char)tolower((unsigned char)e[k]);
        if (e == b) return i;
        if (t == StepDraw && s == W3dPrimitiveName(i)) return i;
        static const char* const kArrayIs[ArrayN] = { "vertices", "normals", "uv", "colors" };   // step("array", "normales", true)
        if (t == StepArray && i < (int)ArrayN && b == kArrayIs[i]) return i;
    }
    return -1;
}
// step(nombre, ...): ejecuta UN paso de rutina, como una llamada de OpenGL. Vale SOLO mientras se dibuja: adentro
// de una funcion que llama el paso "Llamar Lua" de una rutina. Los argumentos se reparten por su tipo, en orden:
//   texto     -> la referencia (malla / objeto / textura / rutina), despues la OPCION ("tira", "Diffuse"...);
//                "@nombre[i]" es una memoria (un numero del paso; en "dibujar", el array de rangos)
//   numero    -> el entero si el paso lo usa (la parte, el numero de luz, cuantos saltear), la opcion si el paso no
//                tiene numeros, y despues los numeros en orden
//   booleano  -> on/off
//   step("punteroVertices", "rival_comun_0")   step("dibujar", "rival_comun_0", 2, "tira")
//   step("trasladar", 0, 2, 0)   step("niebla", true)   step("lightColor", 0, "Diffuse", 1, 0.9, 0.8, 1)
// ---- los PASOS de una rutina desde el juego (su indice: 1 = el primero de la lista que usa la rutina) ----
//   setStep(rutina, i, "on", true|false)          prende / apaga
//   setStep(rutina, i, "n", v1 [, v2 ...])         sus numeros (nil = ese queda); "color" es lo mismo: r, g, b, a
//   setStep(rutina, i, "ref", "nombre")            otra malla / textura / objeto / rutina (error si no existe)
//   setStep(rutina, i, "modo" | "entero", n)       la opcion (la primitiva, el modo de mezcla...) / la parte, la luz...
//   getStep(rutina, i, campo) -> lo mismo que se escribe ("n" / "color" devuelve los numeros)
static W3dStep* StepArg(lua_State* L, Routine** rOut) {
    Object* o = W3dScriptParamObject(L, 1);
    if (!o || o->getType() != ObjectType::routine) { luaL_error(L, "no es una rutina"); return NULL; }
    Routine* r = (Routine*)o;
    std::vector<W3dStep>& list = r->CurrentList();
    const int i = (int)luaL_checkinteger(L, 2) - 1;
    if (i < 0 || i >= (int)list.size()) { luaL_error(L, "la rutina '%s' no tiene el paso %d", r->name.c_str(), i + 1); return NULL; }
    if (rOut) *rOut = r;
    return &list[(size_t)i];
}
static int LSetStep(lua_State* L) {
    Routine* r = NULL;
    W3dStep* p = StepArg(L, &r);
    if (!p) return 0;
    const std::string field = luaL_checkstring(L, 3);
    if (field == "on") p->on = lua_toboolean(L, 4) != 0;
    else if (field == "n" || field == "color") {
        for (int k = 0; k < 5; k++)
            if (!lua_isnoneornil(L, 4 + k)) { p->n[k].v = (float)luaL_checknumber(L, 4 + k); p->n[k].ref.clear(); p->n[k].p = NULL; }
    }
    else if (field == "mode") p->mode = (int)luaL_checkinteger(L, 4);
    else if (field == "integer") p->integer = (int)luaL_checkinteger(L, 4);
    else if (field == "ref") {
        const std::string prev = p->ref;
        p->ref = luaL_checkstring(L, 4);
        r->Resolve();
        const int rt = W3dStepRef(p->type);
        if (rt != RefNothing && rt != RefFunction && !p->ptr) {
            const std::string request = p->ref;
            p->ref = prev; r->Resolve();
            return luaL_error(L, "setPaso: no existe '%s'", request.c_str());
        }
    }
    else return luaL_error(L, "setPaso: campo '%s' (on, n, color, ref, modo, entero)", field.c_str());
    g_w3dRoutinesGen++;   // (el editor la revalida)
    return 0;
}
static int LReadStep(lua_State* L) {
    W3dStep* p = StepArg(L, NULL);
    if (!p) return 0;
    const std::string field = luaL_checkstring(L, 3);
    if (field == "on") { lua_pushboolean(L, p->on ? 1 : 0); return 1; }
    if (field == "mode") { lua_pushinteger(L, p->mode); return 1; }
    if (field == "integer") { lua_pushinteger(L, p->integer); return 1; }
    if (field == "ref") { lua_pushstring(L, p->ref.c_str()); return 1; }
    if (field == "n" || field == "color") {
        const int nn = W3dStepNumbers(*p);
        for (int k = 0; k < nn; k++) lua_pushnumber(L, p->n[k].Value());
        return nn;
    }
    return luaL_error(L, "leerPaso: campo '%s' (on, n, color, ref, modo, entero)", field.c_str());
}
static int LStep(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const int t = W3dStepFromName(name);
    if (t < 0) return luaL_error(L, "paso(): no existe el paso '%s'", name);
    W3dStep p = W3dNewStep(t);
    const int rt = W3dStepRef(t), opt = W3dStepOptions(t);
    const bool usesInteger = W3dStepInteger(t) != IntegerNothing;
    bool refPlaced = false, integerPlaced = false, optPlaced = false;
    int k = 0;
    const int top = lua_gettop(L);
    for (int a = 2; a <= top; a++) {
        const int type = lua_type(L, a);
        if (type == LUA_TBOOLEAN) { p.on = lua_toboolean(L, a) != 0; continue; }
        if (type == LUA_TSTRING) {
            const std::string s = lua_tostring(L, a);
            if (!s.empty() && s[0] == '@') {
                if (t == StepDraw) { p.n[2].ref = s; p.sub = RangeArray; }
                else if (k < 5) p.n[k++].ref = s;
                continue;
            }
            if (t == StepClear) {   // step("limpiar", "color", "profundidad"): los buffers por nombre
                const int bit = (s == "color") ? (int)ClearColor :
                                (s == "depth" || s == "z") ? (int)ClearDepth :
                                (s == "stencil") ? (int)ClearStencil : 0;
                if (!bit) {   // si no es un buffer: la opcion ("auto"...) o la imagen de un fondo propio
                    const int o = optPlaced ? -1 : OptionLua(t, s);
                    if (o >= 0) { p.mode = o; optPlaced = true; continue; }
                    if (!refPlaced) { p.ref = s; refPlaced = true; if (!optPlaced) p.mode = ClearBackgroundOwn; continue; }
                    return luaL_error(L, "paso('limpiar'): '%s' no es un buffer (color, profundidad, estencil)", s.c_str());
                }
                if (!integerPlaced) { p.integer = 0; integerPlaced = true; }
                p.integer |= bit;
                continue;
            }
            if (rt != RefNothing && !refPlaced) { p.ref = s; refPlaced = true; continue; }
            if ((t == StepVisible || t == StepLua) && p.ref2.empty()) { p.ref2 = s; continue; }
            if (opt != OptNothing && !optPlaced) {
                const int o = OptionLua(t, s);
                if (o < 0) return luaL_error(L, "paso('%s'): no existe la opcion '%s'", name, s.c_str());
                p.mode = o; optPlaced = true; continue;
            }
            return luaL_error(L, "paso('%s'): no se que hacer con '%s'", name, s.c_str());
        }
        if (type == LUA_TNUMBER) {
            const float v = (float)lua_tonumber(L, a);
            if (usesInteger && !integerPlaced) { p.integer = (int)v; integerPlaced = true; continue; }
            if (opt != OptNothing && !optPlaced && W3dStepNumbers(p) == 0 && t != StepDraw) {
                p.mode = (int)v; optPlaced = true; continue;
            }
            if (t == StepDraw) {   // despues de la parte: el rango manual (primero, ultimo)
                if (k < 2) { p.n[k++].v = v; p.sub = RangeManual; }
                continue;
            }
            if (k < 5) p.n[k++].v = v;
            continue;
        }
    }
    // ("colorLimpieza" con sus numeros: ESE color, como glClearColor; sin numeros ni opcion, el Auto)
    if (t == StepColorClear && k > 0 && !optPlaced) p.mode = ColorClearOwn;
    std::string error;
    if (!W3dRoutineStepFromLua(p, error)) return luaL_error(L, "paso('%s'): %s", name, error.c_str());
    return 0;
}

static int LPosPx(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float w, h; CanvasSize(&w, &h);
    if (!o) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    lua_pushnumber(L, o->pos.x * w); lua_pushnumber(L, o->pos.y * h); return 2;
}
// setPosPx(o, x, y): pone el objeto ahi. Un eje en NIL se DEJA COMO ESTA:
//   setPosPx(pala, nil, dy) mueve la pala solo en Y (no hay que leer la X para conservarla)
//   setPosPx(o, x, nil) al reves. setPosPx(o, nil, nil) no hace nada.
static int LSetPosPx(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float w, h; CanvasSize(&w, &h);
    if (o && w > 0.0f && h > 0.0f) {
        if (!lua_isnoneornil(L, 2)) o->pos.x = (float)luaL_checknumber(L, 2) / w;
        if (!lua_isnoneornil(L, 3)) o->pos.y = (float)luaL_checknumber(L, 3) / h;
        g_redraw = true;
    }
    return 0;
}
static int LSizePx(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float w, h; CanvasSize(&w, &h);
    if (o && UI2D_IsElement2D(o)) {
        float aw, ah;
        DeclaredSize((Element2D*)o, w, h, &aw, &ah);
        lua_pushnumber(L, aw);
        lua_pushnumber(L, ah);
    } else { lua_pushnumber(L, 0); lua_pushnumber(L, 0); }
    return 2;
}
static int LSetSizePx(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float w, h; CanvasSize(&w, &h);
    if (o && UI2D_IsElement2D(o) && w > 0.0f && h > 0.0f) {
        Element2D* e = (Element2D*)o;
        float nw = (float)luaL_checknumber(L, 2), nh = (float)luaL_checknumber(L, 3);
        // se recibe en px de lienzo y se guarda EN LA UNIDAD del elemento
        if (e->sizeMode == TAM2D_PX) { e->width = nw; e->height = nh; }
        else if (e->sizeMode == TAM2D_SCALED) {
            float m = (w < h) ? w : h;
            float f = (m >= 1.0f) ? m / 480.0f : 1.0f;
            e->width = nw / f; e->height = nh / f;
        }
        else { e->width = nw / w; e->height = nh / h; }
        g_redraw = true;
    }
    return 0;
}
static int LSetText(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (!o) return 0;
    const char* s = lua_tostring(L, 2);   // numeros se convierten solos
    if (o->getType() == ObjectType::text2d) {
        ((Text2D*)o)->text = s ? s : "";
        g_redraw = true;
    } else if (o->getType() == ObjectType::button2d) {
        // el rotulo de un BOTON tambien se cambia por script (idiomas del menu)
        ((Button2D*)o)->text = s ? s : "";
        g_redraw = true;
    }
    return 0;
}
// setTexture(objeto, "ruta.png"): cambia la imagen de un objeto tipo imagen (prender/apagar el
// parlante, el coleccionable del HUD girando). La ruta RELATIVA se resuelve como en sound(): cuelga de
// la carpeta del PROYECTO (g_w3dDirProject). Las rutas del .w3dui se resuelven al CARGAR
// (UI2DFormato::PathOnLoad), pero las que llegan por script entraban CRUDAS y caian al cwd:
// ninguna textura. Si hay contenedor v4 montado y la ruta es una ENTRADA suya, queda tal cual
// (ReadFileBytes la resuelve por el montaje, mismo contrato que el resto de los assets).
// El decode ya se cachea POR RUTA (Texture2DGet, fallos incluidos): ciclar 14 PNG de una
// animacion decodifica cada una UNA vez y despues es solo un bind.
// ---- isDrawn(obj): una malla que dibuja una RUTINA (Mesh::onlyRoutine) se dibujo en este cuadro o en el anterior
//      (paso su prueba de visibilidad). Lo que no se ve no necesita su tick visual (girar, recolorear, bobbear).
//      Cualquier otro objeto: true (no se sabe: el que pregunta hace lo de siempre) ----
static int LDrawn(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    bool d = (o != NULL);
    if (o && o->getType() == ObjectType::mesh && ((Mesh*)o)->onlyRoutine)
        d = (g_w3dPass - ((Mesh*)o)->drawnPass) <= 1u;
    lua_pushboolean(L, d ? 1 : 0);
    return 1;
}
static int LSetTexture(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    const char* s = lua_tostring(L, 2);
    if (o && o->getType() == ObjectType::image2d && s && W3dIsCapturePath(s)) {
        ((Image2D*)o)->texture = s;   // (una captura de pantalla: no es una ruta del proyecto)
        g_redraw = true;
    } else if (o && o->getType() == ObjectType::image2d && s) {
        // (un script de una LIBRERIA externa: primero la de SU libreria, W3dScriptLibPath)
        const std::string fromLib = W3dScriptLibPath(L, s);
        std::string res = fromLib.empty() ? std::string(s) : fromLib;
        bool absolute = !fromLib.empty() || (!res.empty() && (res[0] == '/' || (res.size() > 1 && res[1] == ':')));
        if (!absolute && !g_w3dDirProject.empty()) {
            W3dStore* alm = W3dMountedStore();
            if (!(alm && alm->Exists(res)))
                res = g_w3dDirProject + "/" + res;
        }
        ((Image2D*)o)->texture = res;
        g_redraw = true;
    }
    return 0;
}
// ---- MEZCLA DE TEXTURAS de un MATERIAL (por nombre): la textura B y cuanto de ella (Material::textureB / mixB;
//      gfx::MixTextures: UNA pasada por multitextura, la unidad 1 interpola B con la base ya iluminada).
//      setTextureB(material, ruta): "" = ninguna; la ruta se resuelve como la de setTexture (una entrada del contenedor
//      o del proyecto); devuelve true si la textura quedo puesta. setTextureMix(material, k): 0..1 (0 = solo la base:
//      la unidad 1 ni se prende). getTextureMix(material) -> k. Cambiar k por cuadro cuesta un glTexEnv. ----
static std::string TexturePathScript(lua_State* L, const char* s) {
    const std::string fromLib = W3dScriptLibPath(L, s);
    std::string res = fromLib.empty() ? std::string(s) : fromLib;
    const bool absolute = !fromLib.empty() || (!res.empty() && (res[0] == '/' || (res.size() > 1 && res[1] == ':')));
    if (!absolute && !g_w3dDirProject.empty()) {
        W3dStore* alm = W3dMountedStore();
        if (!(alm && alm->Exists(res))) res = g_w3dDirProject + "/" + res;
    }
    return res;
}
// LA TEXTURA B AHORA ES UNA CAPA DE TEXTURA Mix con su factor (al abrir un proyecto viejo se convierte): sin toon,
// setTextureB / setTextureMix / getTextureMix van a la PRIMERA capa Mix del material (la crean adelante si no hay).
// Con toon, a la textura B de siempre (la rampa). Asi un juego hecho antes sigue andando con el proyecto ya convertido
static TexLayer* LayerMixScript(Material* mt, bool create) {
    if (!mt || mt->toon != 0) return NULL;
    for (size_t c = 0; c < mt->layers.size(); c++) if (mt->layers[c].blend == W3D_LAYER_MIX) return &mt->layers[c];
    if (!create) return NULL;
    TexLayer tl; tl.blend = W3D_LAYER_MIX; tl.force = mt->mixB;
    mt->layers.insert(mt->layers.begin(), tl);
    return &mt->layers[0];
}
static int LSetTextureB(lua_State* L) {
    Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    const char* s = lua_tostring(L, 2);
    if (!mt) { lua_pushboolean(L, 0); return 1; }
    if (!s || !*s) {
        if (mt->toon == 0) {   // (la capa Mix se va)
            for (size_t c = 0; c < mt->layers.size(); c++)
                if (mt->layers[c].blend == W3D_LAYER_MIX) { TextureSetIn(mt->layers[c].tex, NULL); mt->layers.erase(mt->layers.begin() + (long)c); break; }
        } else TextureSetIn(mt->textureB, NULL);
        g_redraw = true; lua_pushboolean(L, 1); return 1;
    }
    Texture* t = TextureFind(s);                  // (ya cargada con esa ruta tal cual: la del .w3d)
    std::string res;
    if (!t) { res = TexturePathScript(L, s); t = TextureFind(res); }
    bool taken = false;
    if (!t) { t = TextureTake(res); taken = true; }
    if (!t) { lua_pushboolean(L, 0); return 1; }
    TexLayer* cap = LayerMixScript(mt, true);
    TextureSetIn(cap ? cap->tex : mt->textureB, t);
    if (taken) TextureRelease(t);   // (la ranura ya tiene su referencia)
    g_redraw = true;
    lua_pushboolean(L, 1);
    return 1;
}
static int LSetMixTexture(lua_State* L) {
    Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    if (!mt) return 0;
    float k = (float)luaL_checknumber(L, 2);
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    TexLayer* cap = LayerMixScript(mt, false);
    float& v = cap ? cap->force : mt->mixB;
    if (v != k) { v = k; g_redraw = true; }
    return 0;
}
static int LMixTexture(lua_State* L) {
    Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    TexLayer* cap = LayerMixScript(mt, false);
    lua_pushnumber(L, cap ? cap->force : (mt ? mt->mixB : 0.0));
    return 1;
}
// ---- LA TRANSFORMACION UV y LA FUERZA DE LA EMISION de un MATERIAL (por nombre; formato/material-animado.md) ----
//      setMaterialUV(material, offU, offV [, scaleU, scaleV [, rot]]): desplazamiento, escala y rotacion (grados) de la
//      UV de la textura base y de los mapas; lo que no se pasa queda como estaba. false = no hay un material con ese
//      nombre. getMaterialUV(material) -> offU, offV, scaleU, scaleV, rot. setEmissionStrength(material, k) /
//      getEmissionStrength(material): la emision es emission x k (1 = la de siempre). Cambiarlas por cuadro no cuesta
//      mas que un numero: la matriz se arma al dibujar (la pantalla con el texto que pasa, sin rutina ni curva)
static int LSetMaterialUV(lua_State* L) {
    Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    if (!mt) { lua_pushboolean(L, 0); return 1; }
    mt->uvOffset[0] = (float)luaL_optnumber(L, 2, mt->uvOffset[0]);
    mt->uvOffset[1] = (float)luaL_optnumber(L, 3, mt->uvOffset[1]);
    mt->uvScale[0]  = (float)luaL_optnumber(L, 4, mt->uvScale[0]);
    mt->uvScale[1]  = (float)luaL_optnumber(L, 5, mt->uvScale[1]);
    mt->uvRotation  = (float)luaL_optnumber(L, 6, mt->uvRotation);
    g_redraw = true;
    lua_pushboolean(L, 1);
    return 1;
}
static int LMaterialUV(lua_State* L) {
    const Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    lua_pushnumber(L, mt ? mt->uvOffset[0] : 0.0);
    lua_pushnumber(L, mt ? mt->uvOffset[1] : 0.0);
    lua_pushnumber(L, mt ? mt->uvScale[0] : 1.0);
    lua_pushnumber(L, mt ? mt->uvScale[1] : 1.0);
    lua_pushnumber(L, mt ? mt->uvRotation : 0.0);
    return 5;
}
static int LSetEmissionStrength(lua_State* L) {
    Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    if (!mt) return 0;
    const float k = (float)luaL_checknumber(L, 2);
    if (mt->emissionStrength != k) { mt->emissionStrength = k; g_redraw = true; }
    return 0;
}
static int LEmissionStrength(lua_State* L) {
    const Material* mt = FindMaterialByName(luaL_checkstring(L, 1));
    lua_pushnumber(L, mt ? mt->emissionStrength : 1.0);
    return 1;
}
// setFlipbook(objeto, cuadros, fps [, cols [, filas [, desfase [, ancho, alto, u0, v0]]]]):
// convierte una Image2D en un FLIPBOOK animado por el MOTOR sobre su textura-atlas. El motor
// avanza la celda cada frame (UpdateFlipbooks); Lua llama esto UNA vez y no toca mas la textura.
// cuadros<=0 lo apaga (vuelve a imagen fija). cols/filas = grilla del atlas (tira horizontal =
// cols=cuadros, filas=1). ancho/alto/u0/v0 opcionales: la SUB-TIRA dentro del atlas unico
// (fraccion + origen; defaults = la textura entera, como siempre).
static int LSetFlipbook(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && o->getType() == ObjectType::image2d) {
        Image2D* im = (Image2D*)o;
        // los enteros van por optNUMBER + truncado: un float por error de un
        // script NO tiene que abortar el tick del juego con "no integer
        // representation" (paso en el N95: el vuelo de la fruta reintentaba
        // cada tick y el spam de errores al log clavaba el aparato)
        int   frames = (int)luaL_optnumber(L, 2, 0.0);
        float fps    = (float)luaL_optnumber(L, 3, 0.0);
        int   cols   = (int)luaL_optnumber(L, 4, (lua_Number)(frames > 0 ? frames : 1));
        int   rows   = (int)luaL_optnumber(L, 5, 1.0);
        int   offset = (int)luaL_optnumber(L, 6, 0.0);
        float width  = (float)luaL_optnumber(L, 7, 1.0);
        float height = (float)luaL_optnumber(L, 8, 1.0);
        float u0     = (float)luaL_optnumber(L, 9, 0.0);
        float v0     = (float)luaL_optnumber(L, 10, 0.0);
        im->FlipbookConfig(frames, fps, cols, rows, offset, width, height, u0, v0);
        g_redraw = true;
    }
    return 0;
}
// setUVRect(objeto, u0, v0, u1, v1): el RECORTE del atlas unico que muestra una
// Image2D FIJA (sin flipbook). (0,0,1,1) = la textura entera, como siempre.
static int LSetUVRect(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && o->getType() == ObjectType::image2d) {
        Image2D* im = (Image2D*)o;
        im->uvRect[0] = (float)luaL_optnumber(L, 2, 0.0);
        im->uvRect[1] = (float)luaL_optnumber(L, 3, 0.0);
        im->uvRect[2] = (float)luaL_optnumber(L, 4, 1.0);
        im->uvRect[3] = (float)luaL_optnumber(L, 5, 1.0);
        g_redraw = true;
    }
    return 0;
}
// sound("sounds/bip.wav" [, vol [, pitch [, loop [, pan]]]]) o sound(ruta, {opciones}) (ver LSound): reproduce un WAV. La ruta relativa
// cuelga de la carpeta del PROYECTO (g_w3dDirProject, la del .w3d abierto); vol 0..1 (default 1);
// pitch 1 = normal (paso fraccional en la voz, W3dSoundPlayPitch). El WAV se carga UNA vez y se
// cachea por ruta resuelta (NULL tambien: un archivo que falta no reintenta cada frame). No
// bloquea: si el mixer no abrio (headless / sin -DW3D_ENABLE_AUDIO) la carga da NULL y no suena.
// cache de WAVs por ruta resuelta (NULL tambien se cachea: un archivo que
// falta no reintenta cada frame). A nivel archivo para que W3dSoundsPreload
// lo pueda llenar ANTES de jugar.
static std::map<std::string, w3dEngine::W3dSound*> gSounds;

// PRECARGA de los WAV del proyecto (sounds/ + music/): el PRIMER disparo de
// cada sample lo cargaba y decodificaba EN PLENO GAMEPLAY -- los picos de
// "Musica:10ms" del [PERF] del N95 eran eso, no el secuenciador. Con el
// contenedor v4 montado se precargan sus entradas .wav; con proyecto de texto,
// los .wav de sounds/ y music/. Se llama al ABRIR el proyecto. (Y los .mp3: el mixer los decodifica igual, ver
// W3dAudio.cpp; antes un .mp3 no se precargaba y el primer Play lo decodificaba en pleno cuadro.)
static bool W3dIsSoundFile(const std::string& n) {
    if (n.size() < 5) return false;
    std::string e = n.substr(n.size() - 4);
    for (size_t i = 0; i < e.size(); i++) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] - 'A' + 'a');
    return e == ".wav" || e == ".mp3";
}
void W3dSoundsPreload() {
    std::vector<std::string> paths;
    W3dStore* alm = W3dMountedStore();
    if (alm) {
        std::vector<std::string> ent;
        alm->List(ent);
        for (size_t i = 0; i < ent.size(); i++)
            if (W3dIsSoundFile(ent[i]))
                paths.push_back(ent[i]);
    } else if (!g_w3dDirProject.empty()) {
        static const char* DIRS[] = { "sounds", "music" };
        for (int d = 0; d < 2; d++) {
            std::string dir = g_w3dDirProject + "/" + DIRS[d];
            std::vector<w3dFileSystem::DirEntry> ents;
            if (!w3dFileSystem::ListDir(dir, ents)) continue;
            for (size_t i = 0; i < ents.size(); i++) {
                const std::string& n = ents[i].name;
                if (!ents[i].isDir && W3dIsSoundFile(n))
                    paths.push_back(dir + "/" + n);
            }
        }
    }
    int newValue = 0;
    for (size_t i = 0; i < paths.size(); i++)
        if (gSounds.find(paths[i]) == gSounds.end()) {
            gSounds[paths[i]] = w3dEngine::W3dSoundLoad(paths[i].c_str());
            newValue++;
        }
    if (newValue) w3dLogf("[CARGA] sonidos precargados: %d (wav/mp3)", newValue);
}

// cierre del proyecto (ver BindsGame.h). W3dSoundFree corta antes las voces que lo usan.
void W3dSoundsFree() {
    for (std::map<std::string, w3dEngine::W3dSound*>::iterator it = gSounds.begin(); it != gSounds.end(); ++it)
        if (it->second) w3dEngine::W3dSoundFree(it->second);
    gSounds.clear();
}
int W3dSoundsCached() { return (int)gSounds.size(); }
bool W3dSoundCached(const std::string& path) { return gSounds.find(path) != gSounds.end(); }

// sound(ruta, {vol=, tono=, loop=, pan=, en=, ataque=, decaimiento=, sostenido=, caida=, liberacion=, duracion=,
// reverb=}): lo mismo con NOMBRES, mas la ENVOLVENTE (segundos y nivel 0..1, ver W3dEnvelope en W3dAudio.h) y el
// ENVIO a la reverb global (reverb(), 0..1). Lo que no se pone queda como en la forma con posiciones
static float FieldNum(lua_State* L, int t, const char* k, float defaultValue, bool* has = 0) {
    lua_getfield(L, t, k);
    float v = defaultValue;
    if (lua_isnumber(L, -1)) { v = (float)lua_tonumber(L, -1); if (has) *has = true; }
    lua_pop(L, 1);
    return v;
}
static int LSound(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const bool options = lua_istable(L, 2);
    float vol   = options ? FieldNum(L, 2, "vol", 1.0f) : (float)luaL_optnumber(L, 2, 1.0);
    float pitch = options ? FieldNum(L, 2, "pitch", 1.0f) : (float)luaL_optnumber(L, 3, 1.0);
    bool  loop  = false;   // arg 4 opcional: true = sample en LOOP (nota sostenida)
    if (options) { lua_getfield(L, 2, "loop"); loop = lua_toboolean(L, -1) != 0; lua_pop(L, 1); }
    else loop = lua_toboolean(L, 4) != 0;
    float pan   = options ? FieldNum(L, 2, "pan", 0.0f) : (float)luaL_optnumber(L, 5, 0.0);   // -1 (izq) .. 0 .. 1 (der)
    // la envolvente (solo si se pidio algo) y el envio a la reverb
    w3dEngine::W3dEnvelope env;
    bool withEnv = false;
    float send   = 0.0f;
    if (options) {
        env.attack   = FieldNum(L, 2, "attack", 0.0f, &withEnv);
        env.decay    = FieldNum(L, 2, "decay", 0.0f, &withEnv);
        env.held     = FieldNum(L, 2, "sustain", 1.0f, &withEnv);
        env.fall     = FieldNum(L, 2, "fall", 0.0f, &withEnv);
        env.release  = FieldNum(L, 2, "release", 0.0f, &withEnv);
        env.duration = FieldNum(L, 2, "duration", 0.0f, &withEnv);
        send         = FieldNum(L, 2, "reverb", 0.0f);
    }
    if (vol < 0.0f) vol = 0.0f; if (vol > 1.0f) vol = 1.0f;
    if (w3dEngine::ConfigMute()) return 0;   // mute global del Core (mismo criterio que beep()); nil = no sono
    // resolver la ruta: absoluta queda tal cual; una ENTRADA del contenedor v4 montado queda
    // tal cual (ReadFileBytes la lee de adentro, y es la MISMA clave con la que
    // W3dSoundsPreload la dejo en gSounds); si no, cuelga del proyecto (archivo suelto).
    // Mismo contrato que setTexture. Antes se colgaba SIEMPRE del proyecto: con los .wav
    // adentro del .w3d esa ruta absoluta no existe en el disco ni es una entrada -> NULL
    // cacheado y el juego quedaba MUDO aunque la precarga los hubiera leido bien.
    // Un script de una LIBRERIA externa busca primero en SU libreria (W3dScriptLibPath).
    std::string res = W3dScriptLibPath(L, path);
    if (res.empty()) res = path;
    else path = 0;   // (ya resuelta)
    if (path && !(res.size() > 0 && (res[0] == '/' || (res.size() > 1 && res[1] == ':')))) {
        W3dStore* alm = W3dMountedStore();
        if (!(alm && alm->Exists(res)) && !g_w3dDirProject.empty())
            res = g_w3dDirProject + "/" + res;
    }
    w3dEngine::W3dSound* s;
    std::map<std::string, w3dEngine::W3dSound*>::iterator it = gSounds.find(res);
    if (it != gSounds.end()) s = it->second;
    else { s = w3dEngine::W3dSoundLoad(res.c_str()); gSounds[res] = s; }
    // DEVUELVE el handle de la voz (> 0) o nil si no sono (WAV faltante, mixer cerrado,
    // 32 voces ocupadas). El handle se le pasa a stopSound(h [, fadeSeg]) — es lo que
    // necesita el secuenciador de musica en lua (notas sostenidas: loop=true + release).
    // arg 6 opcional: el momento EXACTO en el reloj del mixer (audioClock()); sin el, suena ya
    double in = options ? (double)FieldNum(L, 2, "at", -1.0f) : luaL_optnumber(L, 6, -1.0);
    int id = s ? w3dEngine::W3dSoundPlayEnv(s, vol, loop, pitch, pan, in, withEnv ? &env : 0, send) : 0;
    if (id <= 0) return 0;   // nil
    lua_pushinteger(L, id);
    return 1;
}
// stopSound(handle [, fadeSeg]): corta la voz devuelta por sound() con una RAMPA
// corta (default ~5 ms; el mixer la baja a cero por muestra -> no clickea). fadeSeg
// mas largo = release musical. handle nil/0/viejo = no-op seguro (los ids de voz no
// se reciclan: cortar una voz que ya termino no toca a la que reuso el slot).
// adjustSound(h, vol [, pitch]): cambia el volumen (y el pitch) de una voz que YA suena, sin
// re-dispararla ni cortarla: para sonidos continuos que siguen al juego (el motor de un auto con
// las RPM). h nil/viejo = no-op. vol 0..1; pitch como en sound().
static int LFitSound(lua_State* L) {
    int id = (int)luaL_optinteger(L, 1, 0);
    if (id <= 0) return 0;
    float vol = (float)luaL_checknumber(L, 2);
    if (vol < 0.0f) vol = 0.0f; if (vol > 1.0f) vol = 1.0f;
    w3dEngine::W3dSoundSetVolume(id, vol);
    if (!lua_isnoneornil(L, 3)) w3dEngine::W3dSoundSetPitch(id, (float)luaL_checknumber(L, 3));
    return 0;
}
// stopSounds(): corta TODAS las voces (musica y efectos) en el acto. Para cambiar de tema o de
// pantalla sin que quede sonando lo anterior.
static int LStopSounds(lua_State*) {
    w3dEngine::W3dSoundStopAll();
    return 0;
}
static int LStopSound(lua_State* L) {
    int id = (int)luaL_optinteger(L, 1, 0);
    float fade = (float)luaL_optnumber(L, 2, 0.005);
    double in = luaL_optnumber(L, 3, -1.0);     // arg 3 opcional: el corte AGENDADO en el reloj del mixer
    if (id > 0) w3dEngine::W3dSoundStopFadeAt(id, fade, in);
    return 0;
}
// releaseSound(h): suelta la nota de la voz: su envolvente pasa a la liberacion (sin envolvente: el corte de ~5 ms)
static int LReleaseSound(lua_State* L) {
    int id = (int)luaL_optinteger(L, 1, 0);
    if (id > 0) w3dEngine::W3dSoundRelease(id);
    return 0;
}
// reverb(sala, amortiguacion, nivel): la REVERB global (0..1 cada uno; nivel 0 = apagada). Cada sonido le manda lo
// que diga su 'reverb' (sound(ruta, {reverb = 0.3}))
static int LReverb(lua_State* L) {
    w3dEngine::W3dAudioReverb((float)luaL_optnumber(L, 1, 0.5), (float)luaL_optnumber(L, 2, 0.5),
                              (float)luaL_optnumber(L, 3, 0.0));
    return 0;
}
// audioClock(): los SEGUNDOS de audio que mezclo el mixer (no avanza en pausa): el reloj de las agendas de sound() y
// stopSound(). Un secuenciador lo usa como base del tiempo de la musica: las notas suenan en la muestra exacta.
static int LClockAudio(lua_State* L) {
    lua_pushnumber(L, w3dEngine::W3dAudioClock());
    return 1;
}
// clockMs(): un reloj en milisegundos (con decimales): para MEDIR lo que cuesta una parte del script. El reloj de
// pared lo pone el editor (getConfig/W3dProfile.cpp: W3dNowMs, alta resolucion; en el N95 el tick del nanokernel); un
// juego compilado no enlaza el perfilador (BindsJuego se comparte): cae a clock(), el tiempo de CPU del proceso
double (*g_w3dClockMsHook)() = 0;
static int LClockMs(lua_State* L) {
    lua_pushnumber(L, g_w3dClockMsHook ? g_w3dClockMsHook() : (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC);
    return 1;
}
// ---- LINEAS (polilineas cerradas suavizadas con Catmull-Rom): la cuenta de un punto en C. En lua era lo mas caro de
// un juego de carreras en el N95 (cada rival, cada cuadro: ~40 multiplicaciones y una docena de lecturas de tabla) ----
// createLine(nombre, {{x, y, z}, ...}) -> id: copia los puntos (la linea es CERRADA: despues del ultimo vuelve al
// primero). El mismo nombre reusa su lugar (abrir el proyecto otra vez no acumula copias).
// linePoint(id, s) -> x, y, z, rumbo, largo, cabeceo: s en tramos (s = 3.25: un cuarto entre el punto 4 y el 5,
// base 1 como la tabla), la curva de Catmull-Rom por los 4 puntos de alrededor. rumbo en grados alrededor de la
// vertical (adelante = (-sin, -cos) en el plano horizontal: x/y de lua, x/z del motor), largo = el largo horizontal
// del tramo, cabeceo = su pendiente en grados.
// EJES: los puntos van y vienen en los de LUA (Z arriba); se guardan en los del MOTOR (W3dLuaAxes al entrar) y las
// cuentas de abajo (rumbo/largo en x/z, pendiente en y) son las de siempre; el punto sale convertido otra vez.
static std::vector<std::vector<double> > gLines;
static std::map<std::string, int> gLineOf;
static int LLineCreate(lua_State* L) {
    const std::string name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int n = (int)lua_rawlen(L, 2);
    std::vector<double> v((size_t)n * 3);
    for (int i = 0; i < n; i++) {
        lua_rawgeti(L, 2, i + 1);
        for (int c = 0; c < 3; c++) {
            lua_rawgeti(L, -1, c + 1);
            v[(size_t)i * 3 + c] = lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
        W3dLuaAxes(v[(size_t)i * 3 + 1], v[(size_t)i * 3 + 2]);   // lua (x,y,z) -> motor (x,z,y)
        lua_pop(L, 1);
    }
    std::map<std::string, int>::iterator it = gLineOf.find(name);
    int id;
    if (it != gLineOf.end()) { id = it->second; gLines[(size_t)id - 1].swap(v); }
    else { gLines.push_back(v); id = (int)gLines.size(); gLineOf[name] = id; }
    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}
static int LLinePoint(lua_State* L) {
    const int id = (int)luaL_checkinteger(L, 1);
    const double s = luaL_checknumber(L, 2);
    if (id < 1 || id > (int)gLines.size() || gLines[(size_t)id - 1].size() < 3) return 0;
    const std::vector<double>& P = gLines[(size_t)id - 1];
    const int n = (int)(P.size() / 3);
    // (las MISMAS cuentas y en el mismo orden que la version de lua: los mismos numeros)
    const double fl = floor(s);
    int i = (int)fmod(fl, (double)n); if (i < 0) i += n;
    const double f = s - fl;
    const double* a = &P[(size_t)i * 3];
    const double* b = &P[(size_t)((i + 1) % n) * 3];
    const double* p0 = &P[(size_t)((i - 1 + n) % n) * 3];
    const double* p3 = &P[(size_t)((i + 2) % n) * 3];
    const double f2 = f * f, f3 = f * f * f;
    const double a1 = a[0], b1 = b[0], p01 = p0[0], p31 = p3[0];
    const double a2 = a[1], b2 = b[1], p02 = p0[1], p32 = p3[1];
    const double a3 = a[2], b3 = b[2], p03 = p0[2], p33 = p3[2];
    const double c1 = 2 * p01 - 5 * a1 + 4 * b1 - p31, d1 = 3 * a1 - p01 - 3 * b1 + p31;
    const double c3 = 2 * p03 - 5 * a3 + 4 * b3 - p33, d3 = 3 * a3 - p03 - 3 * b3 + p33;
    const double px = 0.5 * (2 * a1 + (b1 - p01) * f + c1 * f2 + d1 * f3);
    double py = 0.5 * (2 * a2 + (b2 - p02) * f + (2 * p02 - 5 * a2 + 4 * b2 - p32) * f2 + (3 * a2 - p02 - 3 * b2 + p32) * f3);
    double pz = 0.5 * (2 * a3 + (b3 - p03) * f + c3 * f2 + d3 * f3);
    double tx = 0.5 * ((b1 - p01) + 2 * c1 * f + 3 * d1 * f2);
    double tz = 0.5 * ((b3 - p03) + 2 * c3 * f + 3 * d3 * f2);
    if (tx * tx + tz * tz < 1e-8) { tx = b1 - a1; tz = b3 - a3; }
    const double gr = 180.0 / 3.141592653589793238462643383279502884;
    const double length = sqrt((b1 - a1) * (b1 - a1) + (b3 - a3) * (b3 - a3));
    W3dLuaAxes(py, pz);   // el punto, de vuelta en los ejes de lua
    lua_pushnumber(L, px); lua_pushnumber(L, py); lua_pushnumber(L, pz);
    lua_pushnumber(L, atan2(-tx, -tz) * gr);
    lua_pushnumber(L, length);
    lua_pushnumber(L, length > 0.01 ? atan((b2 - a2) / length) * gr : 0.0);
    return 6;
}

// luaProfile(cuadros [, etiqueta [, instrucciones [, lineas]]]): PERFIL POR MUESTREO del script que lo llama (diagnostico, tambien en
// el N95). Se llama al EMPEZAR cada update(): marca el borde del cuadro. Cada `instrucciones` instrucciones de la VM
// (1000 por defecto) un hook carga el tiempo de pared transcurrido a la LINEA que corre (tiempo propio), a su FUNCION
// (propio) y a cada funcion de la pila (inclusivo); las funciones van por la linea donde se definen. Lo que tarda una
// funcion de C (colGround, setPosition...) cae en la linea que la llamo o en la siguiente. Cada `cuadros` cuadros vuelca
// al log lo mas caro en ms por cuadro y arranca de cero; si cambia la etiqueta (la fase del juego) arranca de cero sin
// volcar la mezcla; `lineas` = cuantas lineas vuelca (30). Prende tambien el perfil de las rutinas (el render: ms por
// tipo de paso y por rutina) y lo vuelca en los mismos cuadros. luaProfile(0) lo apaga. (Un solo perfil a la vez: el del ultimo script que lo pidio.)
namespace {
struct ProfileLua {
    std::map<int, double> line, own, inclusive;
    std::string label;
    double last, total;
    int frames, each, top;
    unsigned call;   // la llamada del motor de la ultima muestra (W3dScript.cpp: g_w3dScriptCalls)
    long samples;    // los hooks que saltaron: x instrucciones = las instrucciones de la VM (no depende del reloj)
    int instr;
    ProfileLua() : last(0.0), total(0.0), frames(0), each(0), top(30), call(0), samples(0), instr(1000) {}
};
ProfileLua gProfileLua;
}
static double ProfileClock() {
    return g_w3dClockMsHook ? g_w3dClockMsHook() : (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}
extern unsigned g_w3dScriptCalls;
static void ProfileHook(lua_State* L, lua_Debug* ar) {
    gProfileLua.samples++;
    // la primera muestra de una llamada nueva del motor trae lo de afuera de lua (el render): no se cuenta
    if (gProfileLua.call != g_w3dScriptCalls) {
        gProfileLua.call = g_w3dScriptCalls;
        gProfileLua.last = ProfileClock();
        return;
    }
    const double dt = ProfileClock() - gProfileLua.last;
    if (lua_getinfo(L, "Sl", ar) && ar->currentline > 0) {
        gProfileLua.line[ar->currentline] += dt;
        gProfileLua.own[ar->linedefined] += dt;
        // inclusivo: cada funcion de lua de la pila UNA vez (la recursion no suma doble)
        int views[16], nv = 0;
        lua_Debug a;
        for (int lv = 0; nv < 16 && lua_getstack(L, lv, &a); lv++) {
            if (!lua_getinfo(L, "S", &a) || a.what[0] == 'C') continue;
            bool already = false;
            for (int i = 0; i < nv; i++) if (views[i] == a.linedefined) { already = true; break; }
            if (!already) { views[nv++] = a.linedefined; gProfileLua.inclusive[a.linedefined] += dt; }
        }
    }
    gProfileLua.total += dt;
    gProfileLua.last = ProfileClock();   // lo que tarda el hook no se cuenta
}
// las n entradas mas caras de un mapa, en lineas cortas (el log del N95 corta cerca de los 256 caracteres)
static void ProfileDump(const char* that, const std::map<int, double>& m, int n, int frames) {
    std::vector<std::pair<double, int> > v;
    for (std::map<int, double>::const_iterator it = m.begin(); it != m.end(); ++it)
        v.push_back(std::make_pair(it->second, it->first));
    std::sort(v.begin(), v.end());
    char b[256];
    int len = snprintf(b, sizeof b, "PERFIL %s:", that);
    for (int i = 0; i < n && i < (int)v.size(); i++) {
        const std::pair<double, int>& e = v[v.size() - 1 - i];
        if (len > 190) { w3dLogf("%s", b); len = snprintf(b, sizeof b, "PERFIL %s:", that); }
        len += snprintf(b + len, sizeof b - len, " %d=%.3f", e.second, e.first / frames);
    }
    w3dLogf("%s", b);
}
// el perfil de las RUTINAS (objects/Routine.h: ms, veces y llamadas GL por tipo de paso, ms por rutina) va con el de lua:
// mismos cuadros, mismo volcado
static void ProfileRoutinesZero() {
    for (int t = 0; t < StepN; t++) { g_routineProfileTimes[t] = 0; g_routineProfileGL[t] = 0; g_routineProfileMs[t] = 0.0; }
    g_routineProfileMsOf.clear(); g_routineProfileTrisOf.clear();
}
static void ProfileRoutinesDump(int frames) {
    std::vector<std::pair<double, int> > v;
    double tot = 0.0;
    for (int t = 0; t < StepN; t++) if (g_routineProfileTimes[t]) {
        // (por veces: el tiempo es solo de los Dibujar)
        v.push_back(std::make_pair((double)g_routineProfileTimes[t], t)); tot += g_routineProfileMs[t];
    }
    std::sort(v.begin(), v.end());
    w3dLogf("PERFIL pasos: %.3f ms/cuadro en los Dibujar (tipo=ms/veces/gl por cuadro; ms solo de dibujar)", tot / frames);
    char b[256];
    int len = snprintf(b, sizeof b, "PERFIL pasos:");
    for (size_t i = v.size(); i-- > 0; ) {
        const int t = v[i].second;
        if (len > 180) { w3dLogf("%s", b); len = snprintf(b, sizeof b, "PERFIL pasos:"); }
        len += snprintf(b + len, sizeof b - len, " %s=%.3f/%.1f/%.1f", W3dStepName(t), g_routineProfileMs[t] / frames,
                        (double)g_routineProfileTimes[t] / frames, (double)g_routineProfileGL[t] / frames);
    }
    w3dLogf("%s", b);
    std::vector<std::pair<double, std::string> > r;
    for (std::map<std::string, double>::const_iterator it = g_routineProfileMsOf.begin(); it != g_routineProfileMsOf.end(); ++it)
        r.push_back(std::make_pair(it->second, it->first));
    std::sort(r.begin(), r.end());
    len = snprintf(b, sizeof b, "PERFIL rutinas (ms/triangulos por cuadro):");
    for (size_t i = r.size(); i-- > 0; ) {
        if (len > 180) { w3dLogf("%s", b); len = snprintf(b, sizeof b, "PERFIL rutinas (ms/triangulos por cuadro):"); }
        len += snprintf(b + len, sizeof b - len, " %s=%.3f/%ld", r[i].second.c_str(), r[i].first / frames,
                        g_routineProfileTrisOf[r[i].second] / frames);
    }
    w3dLogf("%s", b);
}
static int LProfileLua(lua_State* L) {
    const int each = (int)luaL_optinteger(L, 1, 300);
    const char* et = luaL_optstring(L, 2, "");
    const int instr = (int)luaL_optinteger(L, 3, 1000);
    const int top = (int)luaL_optinteger(L, 4, 30);
    ProfileLua& p = gProfileLua;
    if (each <= 0) { lua_sethook(L, NULL, 0, 0); p = ProfileLua(); g_routineProfile = false; return 0; }
    if (p.each == 0 || p.label != et) {
        p = ProfileLua(); p.label = et; p.each = each; p.top = top; p.instr = instr > 0 ? instr : 1000;
        lua_sethook(L, ProfileHook, LUA_MASKCOUNT, p.instr);
        ProfileRoutinesZero(); g_routineProfile = true;
    } else if (++p.frames >= each) {
        w3dLogf("PERFIL lua %s: %d cuadros, %.3f ms/cuadro muestreados, ~%ld instrucciones de la VM por cuadro "
                "(lineas de auto.lua=ms por cuadro)", p.label.c_str(), p.frames, p.total / p.frames,
                p.samples * (long)p.instr / p.frames);
        ProfileDump("lines", p.line, p.top, p.frames);
        ProfileDump("self", p.own, 16, p.frames);
        ProfileDump("inclusive", p.inclusive, 16, p.frames);
        ProfileRoutinesDump(p.frames); ProfileRoutinesZero();
        const std::string e = p.label;
        const int ins = p.instr;
        p = ProfileLua(); p.label = e; p.each = each; p.top = top; p.instr = ins;
    }
    p.last = ProfileClock();
    p.call = g_w3dScriptCalls;
    return 0;
}
// soundPosition(handle): SEGUNDOS del sonido ya reproducidos por esa voz, o nil si la voz
// no existe (termino, se corto o el handle es nil/viejo). Es el RELOJ MAESTRO para una
// cinematica con audio: el mixer avanza en tiempo real aunque el render vaya a 5 fps, asi
// que el lua hace t = soundPosition(h) y la animacion SALTA los frames que haga falta
// para quedar donde va el sonido (frameskip) en vez de irse quedando atras del audio.
static int LSoundPosition(lua_State* L) {
    int id = (int)luaL_optinteger(L, 1, 0);
    float seg = (id > 0) ? w3dEngine::W3dSoundPos(id) : -1.0f;
    if (seg < 0.0f) return 0;   // nil
    lua_pushnumber(L, seg);
    return 1;
}
// setSector(obj, s): sector ACTIVO del modificador "Culling" (PVS por triangulo) de la
// malla. s es 1-based; 0 o fuera de rango = malla completa (fallback). El lua del juego
// lo llama al cambiar el nodo/camara del riel; el swap de indices es una sola re-subida
// del IBO (nunca trabajo por frame). Objeto sin el modificador / no-malla = no-op.
// HOOK y no un extern directo: este .cpp se comparte con el RUNTIME del juego compilado,
// que NO linkea el editor (main/edit). El editor lo instala (MeshEdit.cpp); sin instalar,
// setSector es un no-op que linkea igual (mismo patron que W3dScriptSetBindExtra).
bool (*W3dPVSSetSectorHook)(Object*, int) = 0;
static int LSetSector(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    int sec = (int)luaL_optinteger(L, 2, 0);
    if (o && W3dPVSSetSectorHook) W3dPVSSetSectorHook(o, sec);
    return 0;
}
// --- VISIBILIDAD POR CELDA (VisSet / VisZone) ---
// visCell(obj): la celda ACTIVA (1-based; 0 = malla completa). obj puede ser la
// malla con el modificador Culling por triangulo o una VisZone. nil = sin dato.
// setVisAnchor(zona, obj): fija el objeto cuya posicion decide la celda en los
// modos grilla/volumenes de la zona (pisa el ancla declarada en el .w3d).
// setVisCurveT(zona, t [, riel]): modo curva: el juego entrega el indice
// FRACCIONARIO del nodo del riel (el mismo ancla que ya calcula para la camara);
// la zona avanza de a UNA celda por tick hacia ese objetivo (el stepper vive en
// la zona). El 3er argumento (opcional) identifica DE QUE riel es ese indice:
// un string con el nombre de la Curve, o la Curve misma. Si la zona declara
// `riel:` y no coincide, cae a su fallback (multi-riel, ver VisZone.h); sin
// declarar riel el identificador se ignora (compatibilidad total).
// Mismo patron de HOOKS que setSector: el editor los instala (MeshEdit/VisZone);
// sin instalar, los binds son no-op seguros que linkean igual.
int  (*W3dVisCellHook)(Object*) = 0;
bool (*W3dVisAnchorHook)(Object*, Object*) = 0;
bool (*W3dVisCurveTHook)(Object*, float, const char*) = 0;
static int LVisCell(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (!o || !W3dVisCellHook) { lua_pushnil(L); return 1; }
    int c = W3dVisCellHook(o);
    if (c < 0) { lua_pushnil(L); return 1; }   // el objeto no participa del sistema
    lua_pushinteger(L, c);
    return 1;
}
static int LSetVisAnchor(lua_State* L) {
    Object* zone = W3dScriptParamObject(L, 1);
    Object* anchor = W3dScriptParamObject(L, 2);
    if (zone && W3dVisAnchorHook) W3dVisAnchorHook(zone, anchor);
    return 0;
}
static int LSetVisCurveT(lua_State* L) {
    Object* zone = W3dScriptParamObject(L, 1);
    float t = (float)luaL_optnumber(L, 2, 0.0);
    // riel (opcional): string con el nombre, o el objeto Curve (se usa su nombre)
    const char* rail = 0;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        if (lua_isstring(L, 3)) rail = lua_tostring(L, 3);
        else { Object* r = W3dScriptParamObject(L, 3); if (r) rail = r->name.c_str(); }
    }
    if (zone && W3dVisCurveTHook) W3dVisCurveTHook(zone, t, rail);
    return 0;
}
// setBatch(coleccion, "estatico"|"off"): prende/apaga el lote estatico de una
// Collection (P4). Mismo patron de hook (lo instala Collection.cpp); no-op seguro.
bool (*W3dSetBatchHook)(Object*, const char*) = 0;
static int LSetBatch(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    const char* mode = luaL_optstring(L, 2, "off");
    if (o && W3dSetBatchHook) W3dSetBatchHook(o, mode);
    return 0;
}
// emit(obj, n): RAFAGA de n particulas del objeto Particulas YA (con cantidad: 0 el
// emisor emite SOLO por rafagas). Hook instalado por el editor (ver BindsGame.h):
// en el runtime compilado queda NULL y el bind es un no-op seguro.
void (*W3dParticlesEmitHook)(Object* o, int n) = 0;
void (*W3dActiveParticlesHook)(Object* o, bool active) = 0;
bool (*W3dFogHook)(Object* o, bool write, float* v, int n) = 0;
// los objetos SONIDO (main/objects/Sound.cpp los instala): sus binds de lua y su tick del juego (este .cpp lo compila
// tambien el runtime; sin el objeto Sonido quedan en NULL y no cuestan nada)
void (*W3dSoundLuaHook)(lua_State* L) = 0;
void (*W3dSoundsGameHook)() = 0;

// LA CAMARA POR HOOK (ver BindsGame.h): los instala main/objects/Camera.cpp.
bool (*W3dCameraProjectHook)(Object*, float, float, float*, float*, bool*) = 0;
bool (*W3dCameraRailSetHook)(Object*, Object*, const char*, bool, const int*) = 0;
bool (*W3dCameraRailNodeHook)(Object*, float) = 0;
bool (*W3dCameraRailLookHook)(Object*, bool) = 0;
bool (*W3dCameraRailStateHook)(Object*, const char**, int*, float*, bool*, float*) = 0;
bool (*W3dCameraLensHook)(Object*, float*, float*, float*) = 0;
bool (*W3dCameraSetLensHook)(Object*, const float*, const float*, const float*) = 0;
bool (*W3dCameraXZHook)(float*, float*, float*, float*) = 0;
bool (*W3dCameraRayHook)(float, float, float, float, float*, float*) = 0;
bool (*W3dCameraProjectPointHook)(float, float, float, float, float, float*, float*, bool*) = 0;
Object* (*W3dTargetHook)(Object*) = 0;
static int LEmit(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    int n = (int)luaL_optinteger(L, 2, 1);
    if (o && n > 0 && W3dParticlesEmitHook) W3dParticlesEmitHook(o, n);
    return 0;
}
// setEmitting(obj, bool): la EMISION continua del emisor (el 'activo' del panel). Apagar un emisor asi no hace
// desaparecer de golpe lo que ya esta en el aire (setVisible si): las vivas terminan su vida y se funden solas.
static int LSetEmitting(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && W3dActiveParticlesHook) W3dActiveParticlesHook(o, lua_toboolean(L, 2) != 0);
    return 0;
}
// getFog(obj) -> densidad, r, g, b, inicio, fin de un objeto Niebla (nil si no es una niebla).
// setFog(obj, densidad [, r, g, b [, inicio, fin]]): cambiarla EN JUEGO (fundir la niebla al entrar a una casa,
// con un fundido de un par de segundos). Lo que no se pasa queda como esta.
static int LFog(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float v[6];
    if (!o || !W3dFogHook || !W3dFogHook(o, false, v, 6)) { lua_pushnil(L); return 1; }
    for (int k = 0; k < 6; k++) lua_pushnumber(L, v[k]);
    return 6;
}
static int LSetFog(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float v[6];
    if (!o || !W3dFogHook || !W3dFogHook(o, false, v, 6)) return 0;
    for (int k = 0; k < 6; k++) if (!lua_isnoneornil(L, 2 + k)) v[k] = (float)luaL_checknumber(L, 2 + k);
    W3dFogHook(o, true, v, 6);
    return 0;
}
// show(obj, visible): prende/apaga el dibujado (para menus del juego).
// ALIAS historico de setVisible() (Core): hace exactamente lo mismo y se mantiene por los scripts
// que ya lo usan. El lector es isVisible(obj), que tambien vive en el Core.
static int LShow(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o) { o->visible = lua_toboolean(L, 2) != 0; g_redraw = true; }
    return 0;
}
// setOpacity(obj, 0..1): opacidad del elemento 2D (se hereda a los hijos -> crossfade de un menu).
static int LSetOpacity(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && UI2D_IsElement2D(o)) {
        float a = (float)luaL_checknumber(L, 2);
        if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
        ((Element2D*)o)->opacity = a;
        g_redraw = true;
    }
    return 0;
}
// setTint(obj, r, g, b [, a]): el color de un elemento 2D (tinte de una imagen, relleno de un rect, color de un
// texto), 0..1. Para un HUD que late o cambia de color (un medidor de vida, un aviso). Sobre un objeto 3D (una malla
// o un vacio con mallas adentro) es el TINTE de sus mallas: multiplica la luz de sus materiales.
static float* ColorOf2D(Object* o) {
    if (!o) return NULL;
    if (o->getType() == ObjectType::image2d) return ((Image2D*)o)->color;
    if (o->getType() == ObjectType::rect2d)   return ((Rect2D*)o)->color;
    if (o->getType() == ObjectType::text2d)  return ((Text2D*)o)->color;
    return NULL;
}
// un objeto 3D: el TINTE de sus mallas (Mesh::tinte, multiplica la luz de sus materiales); si es un vacio o un
// grupo, el de todas las mallas que cuelgan de el. setTint(auto, 0.5, 0.5, 0.5) lo oscurece a la mitad.
static void TintMeshes(Object* o, float r, float g, float b) {
    if (!o) return;
    if (o->getType() == ObjectType::mesh) { Mesh* m = (Mesh*)o; m->tint[0] = r; m->tint[1] = g; m->tint[2] = b; }
    for (size_t i = 0; i < o->Childrens.size(); i++) TintMeshes(o->Childrens[i], r, g, b);
}
static int LSetTint(lua_State* L) {
    Object* o3 = W3dScriptParamObject(L, 1);
    if (o3 && !UI2D_IsElement2D(o3) && o3->getType() != ObjectType::ui) {
        float t[3];
        for (int k = 0; k < 3; k++) { float v = (float)luaL_checknumber(L, 2 + k); t[k] = v < 0.0f ? 0.0f : v; }
        TintMeshes(o3, t[0], t[1], t[2]);
        g_redraw = true;
        return 0;
    }
    float* c = ColorOf2D(o3);
    if (!c) return 0;
    for (int k = 0; k < 4; k++) {
        if (k == 3 && lua_isnoneornil(L, 5)) break;
        float v = (float)luaL_checknumber(L, 2 + k);
        c[k] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
    g_redraw = true;
    return 0;
}
// setRot2D(obj, grados): la rotacion de un elemento 2D (gira alrededor de su ancla; los hijos la heredan).
static int LSetRot2D(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && UI2D_IsElement2D(o)) { ((Element2D*)o)->rot2d = (float)luaL_checknumber(L, 2); g_redraw = true; }
    return 0;
}
// setSize(obj, px): tamano de fuente de un texto 2D.
static int LSetSize(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && o->getType() == ObjectType::text2d) {
        ((Text2D*)o)->size = (float)luaL_checknumber(L, 2);
        g_redraw = true;
    }
    return 0;
}
// quit(): pide cerrar el juego (la plataforma corta el loop). En el editor no hace efecto visible.
static int LExit(lua_State*) { gQuit = true; return 0; }

// pressed(objeto) -> bool. true SOLO en el frame del tap/click NUEVO (flanco de bajada) cuyo punto
// cae DENTRO del rect en pantalla del objeto. Vale para mouse Y para cualquier dedo (multitouch).
// El rect es el RESUELTO por el layout (anclas/flex, como uiBox): un boton dentro de un contenedor
// se toca DONDE SE DIBUJA; si aun no se dibujo cae al rect crudo (BoxHitCanvas).
static int LPressed(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float cx, cy, hw, hh;
    if (!o || !BoxHitCanvas(o, &cx, &cy, &hw, &hh)) { lua_pushboolean(L, 0); return 1; }
    bool newValue = false;
    for (int i = 0; i < W3D_GAME_MAXDEDOS; i++) {
        if (gPunAct[i] && !gPunPrev[i]) {   // recien apretado este frame
            float px = gPunX[i], py = gPunY[i];
            if (px >= cx - hw && px <= cx + hw && py >= cy - hh && py <= cy + hh) { newValue = true; break; }
        }
    }
    lua_pushboolean(L, newValue ? 1 : 0);
    return 1;
}
// collide(a, b) -> bool. true si los rects EN PANTALLA de a y b se solapan (AABB).
// Cada operando usa la regla crudo-vs-resuelto (BoxCollision): los objetos de
// gameplay (hijos directos de la raiz, ancla centro, layout libre) se comparan
// por su pos CRUDA de este frame (sin lag: un setPosPx del mismo frame ya
// cuenta), y los puestos por el layout (flex / ancla a borde) por su rect
// RESUELTO, que es donde se dibujan (antes se usaba el crudo para todos y
// chocan no coincidia con uiBox ni con lo que se veia).
static int LCollide(lua_State* L) {
    Object* a = W3dScriptParamObject(L, 1);
    Object* b = W3dScriptParamObject(L, 2);
    float ax, ay, ahw, ahh, bx, by, bhw, bhh;
    if (!BoxCollision(a, &ax, &ay, &ahw, &ahh) || !BoxCollision(b, &bx, &by, &bhw, &bhh)) {
        lua_pushboolean(L, 0); return 1;
    }
    bool hit = fabsf(ax - bx) <= (ahw + bhw) && fabsf(ay - by) <= (ahh + bhh);
    lua_pushboolean(L, hit ? 1 : 0);
    return 1;
}
// limit(objeto, area [, eje]): mueve el objeto para que su rect no se salga de la caja de
//   'area' (la que da uiBox, o sea la YA resuelta por el layout). Es la forma recomendada:
//   el juego no calcula ni lee ningun borde, el .w3dui manda.
// limit(objeto, min, max [, eje]): la misma idea con los dos bordes a mano (px de lienzo);
//   con eje "x" son izquierda/derecha y con "y" arriba/abajo.
//
// 'eje' es "xy" (default), "x" o "y" -- la misma convencion de nombres de eje que ya usa
// bounceOn(obj, area, "arriba abajo") del Core. Antes esta funcion recortaba SOLO en Y: el
// nombre y la doc eran genericos ("que su rect no se salga") pero la semantica era "clamp
// vertical", heredada de la pala de un pong. Un objeto que se sale por un costado no tenia
// como acotarse.
// El objeto MOVIL usa la MISMA regla crudo-vs-resuelto que chocan (BoxCollision):
//   * autoritativo (pala del pong): caja CRUDA de este frame -> el clamp del mismo
//     frame que el setPosPx, sin lag (identico a como era siempre);
//   * puesto por el layout (ancla a borde / dentro de un flex): caja RESUELTA (la
//     dibujada) y el ajuste se aplica como DELTA sobre su pos, en la fraccion de
//     su rect de referencia (RefCanvasHeight) -> se corre lo justo, sin teletransporte
//     (antes se le pisaba pos.y con un centro crudo que no era el suyo). En una
//     celda de filas/columnas la pos se ignora al dibujar: el delta no tiene efecto
//     visible (el layout manda; documentado a proposito).
static int LLimit(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float w, h; CanvasSize(&w, &h);
    float cx, cy, hw, hh;
    if (!o || !BoxCollision(o, &cx, &cy, &hw, &hh) || h <= 0.0f) return 0;
    Object* area = W3dScriptParamObject(L, 2);
    // el eje es el ULTIMO argumento y es texto: con area va en 3, con dos bordes en 4
    const char* axis = luaL_optstring(L, area ? 3 : 4, "xy");
    bool inX = false, inY = false;
    for (const char* p = axis; p && *p; p++) { if (*p == 'x' || *p == 'X') inX = true;
                                             if (*p == 'y' || *p == 'Y') inY = true; }
    if (!inX && !inY) { inX = true; inY = true; }   // texto raro: el default
    float left = 0, right = 0, up = 0, down = 0;
    if (area) {
        float acx, acy, aw, ah;
        if (!BoxCanvas(area, &acx, &acy, &aw, &ah) || ah <= 0.0f) return 0;
        left = acx - aw * 0.5f; right   = acx + aw * 0.5f;
        up   = acy - ah * 0.5f; down = acy + ah * 0.5f;
    } else {
        // dos numeros: son los bordes del eje pedido. Con "xy" (el default) se
        // interpretan como arriba/abajo, que es lo que hacia siempre.
        const float a = (float)luaL_checknumber(L, 2);
        const float b = (float)luaL_checknumber(L, 3);
        if (inX && !inY) { left = a; right = b; }
        else             { up = a; down = b; inY = true; inX = false; }
    }
    if (inY) {
        float newValue = cy;
        if (newValue - hh < up) newValue = up + hh;         // no pasar el borde de arriba
        if (newValue + hh > down)  newValue = down  - hh;   // ni el de abajo
        if (RawPosAuthoritative(o)) {
            o->pos.y = newValue / h;   // fraccion del lienzo, como setPosPx (camino de siempre)
        } else {
            float refH;
            if (!RefCanvasHeight(o, &refH)) refH = h;   // sin resuelto: cae al lienzo (como el crudo)
            o->pos.y += (newValue - cy) / refH;         // delta en la unidad de SU rect de referencia
        }
    }
    if (inX && w > 0.0f) {
        float newValue = cx;
        if (newValue - hw < left) newValue = left + hw;
        if (newValue + hw > right) newValue = right - hw;
        if (RawPosAuthoritative(o)) {
            o->pos.x = newValue / w;
        } else {
            float refW;
            if (!RefCanvasWidth(o, &refW)) refW = w;
            o->pos.x += (newValue - cx) / refW;
        }
    }
    g_redraw = true;
    return 0;
}
// inside(area, x, y) -> bool: true si el punto cae DENTRO de la caja del objeto 'area' (la de uiBox,
//   ya resuelta por el layout). Un eje en NIL no se mira: inside(cancha, x) pregunta solo si esta
//   entre los costados. Sirve para las reglas que se escriben contra una zona del layout ("el dedo
//   cayo en la cancha", "la pelota se fue por un costado") sin derivar ni un borde en el script.
static int LInside(lua_State* L) {
    Object* area = W3dScriptParamObject(L, 1);
    float cx, cy, aw, ah;
    if (!area || !BoxCanvas(area, &cx, &cy, &aw, &ah)) { lua_pushboolean(L, 0); return 1; }
    bool ok = true;
    if (!lua_isnoneornil(L, 2)) {
        float x = (float)luaL_checknumber(L, 2);
        if (x < cx - aw * 0.5f || x > cx + aw * 0.5f) ok = false;
    }
    if (ok && !lua_isnoneornil(L, 3)) {
        float y = (float)luaL_checknumber(L, 3);
        if (y < cy - ah * 0.5f || y > cy + ah * 0.5f) ok = false;
    }
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}
// getScale() -> factor responsive = min(screenWidth, screenHeight) / 480.
// getScale(objeto) -> sx, sy, sz: la escala del OBJETO (el mismo bind del Core, script/W3dScript.cpp), en los ejes de
// LUA (Z arriba): sy es la scale.z del motor y sz la scale.y (W3dLuaAxes), igual que el del Core.
// Las dos formas conviven en un solo nombre porque este registro corre DESPUES del Core y lo pisa:
// si viene un objeto se responde la escala del objeto, si no el factor de pantalla de siempre.
static int LScale(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o) {
        float sy = o->scale.y, sz = o->scale.z;
        W3dLuaAxes(sy, sz);
        lua_pushnumber(L, o->scale.x); lua_pushnumber(L, sy); lua_pushnumber(L, sz);
        return 3;
    }
    float w, h; CanvasSize(&w, &h);
    float m = (w < h) ? w : h;
    lua_pushnumber(L, m / 480.0f);
    return 1;
}
// safeArea() -> x, y, ancho, alto: el rect seguro (sin notch/barras) en coords de LIENZO, con (x,y)
// en la ESQUINA superior-izquierda. Antes del primer render (o sin zona segura) cae al lienzo entero.
static int LSafeArea(lua_State* L) {
    float lw, lh; CanvasSize(&lw, &lh);
    float x = -lw * 0.5f, y = -lh * 0.5f, aw = lw, ah = lh;
    if (gLastEsc > 0.0001f && gSafeWinW > 0.0f && gSafeWinH > 0.0f) {
        x  = (gSafeWinX - gLastX0) / gLastEsc - lw * 0.5f;
        y  = (gSafeWinY - gLastY0) / gLastEsc - lh * 0.5f;
        aw = gSafeWinW / gLastEsc;
        ah = gSafeWinH / gLastEsc;
    }
    lua_pushnumber(L, x); lua_pushnumber(L, y);
    lua_pushnumber(L, aw); lua_pushnumber(L, ah);
    return 4;
}
// fade(objeto, objetivo, paso): acerca la opacidad del objeto hacia objetivo (0..1) a lo sumo 'paso'.
static int LMerge(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    if (o && UI2D_IsElement2D(o)) {
        float obj  = (float)luaL_checknumber(L, 2);
        float step = (float)luaL_checknumber(L, 3);
        if (obj < 0.0f) obj = 0.0f; if (obj > 1.0f) obj = 1.0f;
        if (step < 0.0f) step = -step;
        Element2D* e = (Element2D*)o;
        float a = e->opacity;
        if (a < obj)      { a += step; if (a > obj) a = obj; }
        else if (a > obj) { a -= step; if (a < obj) a = obj; }
        e->opacity = a;
        g_redraw = true;
    }
    return 0;
}
// uiBox(objeto) -> x, y, ancho, alto: el rect YA RESUELTO por el layout (anclas/flexbox) en coords de
// LIENZO, con (x,y) = CENTRO (como getPosPx) y (ancho,alto) = tamano (como getSizePx). Si el objeto no se dibujo
// aun (o esta oculto) cae a la pos/tam crudos, como getPosPx/getSizePx.
static int LBoxUI(lua_State* L) {
    float cx = 0.0f, cy = 0.0f, aw = 0.0f, ah = 0.0f;
    BoxCanvas(W3dScriptParamObject(L, 1), &cx, &cy, &aw, &ah);   // false (nil) -> ceros
    lua_pushnumber(L, cx); lua_pushnumber(L, cy);
    lua_pushnumber(L, aw); lua_pushnumber(L, ah);
    return 4;
}

// screenOf(obj) -> x, y, delante: la posicion 3D del objeto PROYECTADA al lienzo
// de la UI, en PIXELES con centro en (0,0) — EXACTAMENTE el espacio de getPosPx/
// setPosPx (x a la derecha, y hacia ABAJO): lo que devuelve se le pasa TAL CUAL a
// setPosPx para que un elemento 2D siga a un objeto 3D (la fruta que vuela hasta
// el contador del HUD: setPosPx(icono, screenOf(fruta))). El tercer retorno
// 'delante' = true si el objeto esta DELANTE de la camara; con false x/y son la
// proyeccion espejada de atras (ocultá el elemento en ese caso).
// Usa la camara ACTIVA del juego (CameraActive) con SU lente y el aspecto del
// render (g_renderAspect, los mismos numeros del modo juego; antes del primer
// render cae al aspecto del lienzo). Sin camara activa devuelve 0, 0, false.
// OJO: con camara ORTOGRAFICA proyecta con la lente en perspectiva (aproximado).
// screenRay(x, y) -> ox, oy, oz, dx, dy, dz: el rayo de la camara ACTIVA que pasa por el punto (x, y)
// del lienzo (px, centro 0,0, y hacia abajo: lo que dan mouse() y tap()). Origen en el plano cercano y
// direccion normalizada, en MUNDO y en los ejes de LUA (Z arriba; el hook da los del motor y se convierten con
// W3dLuaAxes): se le pasa tal cual a colRay / rayMesh. Es la "pistola de luz" de un
// juego de disparos: clic -> rayo -> lo primero que toca. Sin camara activa devuelve nil.
static int LRayScreen(lua_State* L) {
    float w, h; CanvasSize(&w, &h);
    float o[3], d[3];
    if (!W3dCameraRayHook || !W3dCameraRayHook((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), w, h, o, d)) {
        lua_pushnil(L);
        return 1;
    }
    W3dLuaAxes(o[1], o[2]);
    W3dLuaAxes(d[1], d[2]);
    for (int i = 0; i < 3; i++) lua_pushnumber(L, o[i]);
    for (int i = 0; i < 3; i++) lua_pushnumber(L, d[i]);
    return 6;
}
// screenPoint(x, y, z) -> sx, sy, delante: como screenOf pero para un PUNTO de mundo (no el origen de un
// objeto): la mira de un enemigo, un impacto, la esquina de una caja. El punto en los ejes de LUA (Z arriba; al hook
// va en los del motor, W3dLuaAxes). Lo que devuelve, en el mismo espacio que getPosPx (2D: no se convierte).
static int LScreenPoint(lua_State* L) {
    float w, h; CanvasSize(&w, &h);
    float sx = 0, sy = 0; bool front = false;
    float px = (float)luaL_checknumber(L, 1), py = (float)luaL_checknumber(L, 2), pz = (float)luaL_checknumber(L, 3);
    W3dLuaAxes(py, pz);
    if (!W3dCameraProjectPointHook || !W3dCameraProjectPointHook(px, py, pz, w, h, &sx, &sy, &front)) {
        lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushboolean(L, 0);
        return 3;
    }
    lua_pushnumber(L, sx); lua_pushnumber(L, sy); lua_pushboolean(L, front ? 1 : 0);
    return 3;
}

static int LScreenOf(lua_State* L) {
    Object* o = W3dScriptParamObject(L, 1);
    float w, h; CanvasSize(&w, &h);
    float sx = 0, sy = 0; bool front = false;
    if (!o || !W3dCameraProjectHook || !W3dCameraProjectHook(o, w, h, &sx, &sy, &front)) {
        lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushboolean(L, 0);
        return 3;
    }
    lua_pushnumber(L, sx);
    lua_pushnumber(L, sy);
    lua_pushboolean(L, front ? 1 : 0);
    return 3;
}

// ---------------------------------------------------------------------------
//  EL RIEL DE LA CAMARA desde lua. Una Camera viaja por una Curve ("riel") que se
//  declara en el .w3d y se elige por nombre (`riel:` de Camera). Hasta ahora ese
//  vinculo era de PROYECTO: se armaba al abrir y no se podia mover. El bonus de
//  Un juego con varias salas necesita moverlo EN JUEGO -el area abierta, el viaje
//  de una plataforma y una sala lateral 2D son tres rieles distintos, y entre dos de
//  ellas puede haber un teletransporte (400443 unidades crudas en el caso medido),
//  asi que no se pueden coser en una sola curva-.
//
//  Los rieles alternativos se DECLARAN en el .w3d como Curve normales y se cambia
//  por objeto o por nombre; no se carga un .cap en caliente (el porque, en
//  Camera::SetRailObject).
//
//    setRail(camara, riel [, offsetRail])   riel = objeto Curve | "Nombre" | nil
//    setRailNode(camara, indice)            indice >= 0 fija el nodo; -1 = seguir al target
//    setRailLook(camara, on)              on = mirar con el yaw/pitch autoral del nodo
//    getRail(camara) -> nombre, offsetRail, nodo, lookRail, indice
// ---------------------------------------------------------------------------
// (los helpers CamaraArg / CurvaArg vivian aca y tenian que conocer Camera y Curve,
//  de ahi el #ifdef. Ahora el tipo lo verifica el que instala el hook.)

// setRail(camara, riel [, offsetRail]) -> true si quedo puesto.
// Cambia el riel por el que viaja la camara. 'riel' es el objeto Curve o su nombre;
// nil SUELTA el riel (la camara se queda donde esta). El tercer argumento, opcional,
// fija de paso el offsetRail EN NODOS (cada riel puede tener el suyo), asi el cambio
// es una sola llamada atomica. La camara se reposiciona YA sobre el riel nuevo -no
// queda un frame en el viejo- y el seguimiento arranca de cero.
static int LSetRail(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    if (!cam || !W3dCameraRailSetHook) { lua_pushboolean(L, 0); return 1; }
    const bool release = lua_isnoneornil(L, 2);
    const char* name = NULL;
    Object* curve = NULL;
    if (!release) {
        if (lua_isstring(L, 2) && !lua_isnumber(L, 2)) name = lua_tostring(L, 2);
        else                                           curve  = W3dScriptParamObject(L, 2);
    }
    int off = 0; const int* poff = NULL;
    if (lua_isnumber(L, 3)) { off = (int)lua_tonumber(L, 3); poff = &off; }
    lua_pushboolean(L, W3dCameraRailSetHook(cam, curve, name, release, poff) ? 1 : 0);
    return 1;
}

// setRailNode(camara, indice) -> true.
// indice >= 0: la camara se PLANTA en ese nodo del riel actual (float: interpola entre
// nodo y nodo) y deja de proyectarse sobre el target. Es lo que pide un tramo donde el
// indice de camara ES el progreso de un recorrido y el ease viene horneado en el
// ESPACIADO de los nodos: muestrear por indice lo respeta, reparametrizar por arco lo
// borraria. indice < 0 (o -1): vuelve al SEGUIMIENTO normal.
static int LSetRailNode(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    if (!cam || !W3dCameraRailNodeHook) { lua_pushboolean(L, 0); return 1; }
    float n = (float)luaL_optnumber(L, 2, -1.0);
    lua_pushboolean(L, W3dCameraRailNodeHook(cam, n) ? 1 : 0);
    return 1;
}

// setRailLook(camara [, on]) -> true.
// on = true (default del argumento): la camara deja de MIRAR al target y toma el
// yaw/pitch/roll AUTORAL grabado en el nodo del riel (interpolado por arco corto). El
// target sigue decidiendo DONDE se para, no hacia donde mira. on = false: look-at de
// siempre, que es el default de todo proyecto.
// Solo tiene efecto si el .cap trae el canal 'r'; sin el, se sigue mirando al target.
static int LSetLookRail(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    if (!cam || !W3dCameraRailLookHook) { lua_pushboolean(L, 0); return 1; }
    const bool on = lua_isnoneornil(L, 2) ? true : (lua_toboolean(L, 2) != 0);
    lua_pushboolean(L, W3dCameraRailLookHook(cam, on) ? 1 : 0);
    return 1;
}

// getRail(camara) -> nombre, offsetRail, nodo, lookRail, indice. Sin riel devuelve
// "", 0, -1, false, -1. Es el getter que usan los asserts del harness y las pruebas
// .w3s. El 5to valor es el INDICE FRACCIONARIO donde la camara quedo parada este
// tick (seguimiento o nodo manual, offset ya restado): es el `t` que una VisZone
// modo curva espera en setVisCurveT(zona, t) para que la playlist de celdas del
// `.w3dvis` avance nodo a nodo con la camara.
static int LRailOf(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    const char* name = ""; int off = 0; float node = -1.0f; bool look = false;
    float index = -1.0f;
    if (cam && W3dCameraRailStateHook)
        W3dCameraRailStateHook(cam, &name, &off, &node, &look, &index);
    lua_pushstring(L, name ? name : "");
    lua_pushnumber(L, off);
    lua_pushnumber(L, node);
    lua_pushboolean(L, look ? 1 : 0);
    lua_pushnumber(L, index);
    return 5;
}

// ---------------------------------------------------------------------------
//  EL LENTE DE LA CAMARA desde lua: fov + planos de recorte.
//
//    getLens(camara) -> fov, cerca, lejos      (grados, unidades de mundo)
//    setLens(camara [, fov] [, cerca] [, lejos]) -> true
//
//  Los tres son propiedades de la camara que ya viven en el .w3d; lo que faltaba
//  era poder moverlas EN JUEGO. El caso tipico del plano lejano es cambiar el
//  alcance del dibujado al entrar a un espacio cerrado, para que no asome
//  geometria lejana que ahi no corresponde -- y de paso ahorrarsela.
//  Cada argumento es opcional: nil = no tocar ese. Se validan igual que el .w3d
//  (fov entre 1 y 179; cerca > 0; lejos > cerca), asi un valor absurdo desde un
//  script no deja la camara sin nada que dibujar.
// ---------------------------------------------------------------------------
// setActiveCamera(cam): esa camara pasa a ser la ACTIVA (la que se ve jugando). Es el mismo camino que la pista
// "Camara activa" de una animacion de escena (g_activeCameraHook, por nombre): sirve para volver a la camara de
// juego al terminar una cinematica. -> true si era una camara.
static int LSetActiveCamera(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    if (!cam || cam->getType() != ObjectType::camera || !g_activeCameraHook) { lua_pushboolean(L, 0); return 1; }
    g_activeCameraHook(cam->name.c_str());
    lua_pushboolean(L, 1);
    return 1;
}
static int LLensOf(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    float fov = 0, near = 0, far = 0;
    if (!cam || !W3dCameraLensHook || !W3dCameraLensHook(cam, &fov, &near, &far)) {
        lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
        return 3;
    }
    lua_pushnumber(L, fov); lua_pushnumber(L, near); lua_pushnumber(L, far);
    return 3;
}
static int LSetLens(lua_State* L) {
    Object* cam = W3dScriptParamObject(L, 1);
    if (!cam || !W3dCameraSetLensHook) { lua_pushboolean(L, 0); return 1; }
    float f = 0, c = 0, l = 0;
    const float* pf = lua_isnumber(L, 2) ? (f = (float)lua_tonumber(L, 2), &f) : NULL;
    const float* pc = lua_isnumber(L, 3) ? (c = (float)lua_tonumber(L, 3), &c) : NULL;
    const float* pl = lua_isnumber(L, 4) ? (l = (float)lua_tonumber(L, 4), &l) : NULL;
    lua_pushboolean(L, W3dCameraSetLensHook(cam, pf, pc, pl) ? 1 : 0);
    return 1;
}

// ---------------------------------------------------------------------------
//  ADAPTADOR 2D de la FISICA del Core (physics/W3dFisica). El Core no conoce el
//  lienzo del juego ni Element2D (viven aca, en main/), asi que le prestamos
//  estas dos funciones: con ellas su fisica trabaja en px de LIENZO, el MISMO
//  espacio que getPosPx/getSizePx/collide, y las cajas de rebote son los mismos rects
//  que compara collide(). Se registra en BindsGameRegister -> lo tienen igual
//  el Play del editor y el juego compilado.
// ---------------------------------------------------------------------------
static void PhysicsCanvas(float* w, float* h) { CanvasSize(w, h); }
static bool PhysicsSize2D(Object* o, float* width, float* height) {
    *width = 0.0f; *height = 0.0f;
    if (!o) return false;
    float w, h; CanvasSize(&w, &h);
    if (o->getType() == ObjectType::ui) {   // la raiz de la escena "mide" todo el lienzo
        *width = w; *height = h; return true;
    }
    if (!UI2D_IsElement2D(o)) return false;
    DeclaredSize((Element2D*)o, w, h, width, height);
    return true;
}
// rect RESUELTO por el layout (lo mismo que uiBox): la fisica lo usa para los objetos que hacen
// de AREA (bounceOn(obj, cancha)) y para los OBSTACULOS no autoritativos de bounce(), que estan
// anclados/acomodados por el .w3dui y por eso no estan donde dice su pos cruda. false = no se
// dibujo aun -> el Core cae a su caja normal.
static bool PhysicsBoxUI(Object* o, float* cx, float* cy, float* width, float* height) {
    return ResolvedRectCanvas(o, cx, cy, width, height);
}
// la pos cruda del objeto es autoritativa? La MISMA regla crudo-vs-resuelto de chocan
// (RawPosAuthoritative): la fisica la usa para elegir la caja del OBSTACULO de
// bounce(obj, otro) -> una pala movida por el script ESTE frame colisiona en su pos
// nueva (sin lag), y un obstaculo puesto por el layout colisiona donde se dibuja.
static bool PhysicsPosRaw(Object* o) {
    return RawPosAuthoritative(o);
}

// ============================================================================
//  TRES BINDS QUE ESTABAN SOLO EN EL EDITOR — Y ESE ERA EL BUG.
//
//  cameraXZ(), target() y param() se registraban en ScriptUI.cpp, que el
//  juego compilado NO linkea. O sea: el MISMO .lua tenia tres funciones menos en
//  el .deb que en el Play. En el demo eso se veia como "el juego compilado no
//  tiene nada que ver con el editor": el controlador del personaje llama a
//  cameraXZ() para mover a Crash relativo a la camara, en el binario era `nil`,
//  y update() reventaba con "attempt to call a nil value" TODOS LOS FRAMES
//  -- o sea que la mitad de abajo del script (sonidos, HUD, gameplay) no corria
//  nunca. Ahora los tres viven aca, en el set COMPARTIDO, que es lo que dice el
//  encabezado de este archivo: un juego corre IGUAL en Play y compilado.
//
//  Los dos que necesitan objetos del editor (la camara activa, el target del
//  objeto Script) entran por HOOK, igual que screenOf/setRail/getRail: los
//  instala quien linkee esos objetos, y donde no haya camara el bind devuelve un
//  default sensato en vez de nil (un juego que lo llama sin guarda NO se cae).
// ============================================================================
// cameraXZ() -> fx, fy, rx, ry: adelante y derecha de la camara en el plano HORIZONTAL, que en lua (Z arriba) es el
// XY. El nombre quedo del plano XZ del MOTOR (Y arriba), donde los calcula el hook: la z del motor es la y de lua, asi
// que los cuatro numeros son los mismos y no hay nada que convertir (W3dLuaAxes no aplica: no hay vertical). Sin
// camara: adelante (0, -1), derecha (1, 0).
static int LCameraXZ(lua_State* L) {
    float fx = 0.0f, fz = -1.0f, rx = 1.0f, rz = 0.0f;   // default: sin camara
    if (W3dCameraXZHook) W3dCameraXZHook(&fx, &fz, &rx, &rz);
    lua_pushnumber(L, fx); lua_pushnumber(L, fz);
    lua_pushnumber(L, rx); lua_pushnumber(L, rz);
    return 4;
}
// target() -> el target del objeto dueno del script; nil si no tiene
static int LTarget(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_owner");
    Object* owner = (Object*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    Object* t = (owner && W3dTargetHook) ? W3dTargetHook(owner) : 0;
    if (t) lua_pushlightuserdata(L, t);
    else   lua_pushnil(L);
    return 1;
}
// param("piso" [, default]) -> alias NUMERICO de property() (ver el
// comentario largo que tenia en ScriptUI.cpp). Se implementa LLAMANDO a
// property() para que las dos no puedan divergir.
static int LParameter(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    const lua_Number def = luaL_optnumber(L, 2, 0.0);
    lua_getglobal(L, "property");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, def); return 1; }
    lua_pushstring(L, n);
    if (lua_pcall(L, 1, 1, 0) != 0) { lua_pop(L, 1); lua_pushnumber(L, def); return 1; }
    if (lua_isnumber(L, -1)) { lua_Number v = lua_tonumber(L, -1); lua_pop(L, 1); lua_pushnumber(L, v); return 1; }
    if (lua_type(L, -1) == LUA_TSTRING) {
        const char* s = lua_tostring(L, -1);
        lua_pop(L, 1);
        if (s && lua_stringtonumber(L, s)) return 1;
        lua_pushnumber(L, def);
        return 1;
    }
    lua_pop(L, 1);
    lua_pushnumber(L, def);
    return 1;
}

// ---- CONTROLES: los binds de consulta desde lua ----------------------------
// controls() -> cuantos mandos hay enchufados AHORA (0 = ninguno: el script
// puede pausar solo). control(n) -> nombre, tipo del n-esimo (1-based, como todo
// en lua); sin ese control devuelve nil. El registro vive mas abajo y lo llenan
// los eventos de hotplug, asi que esto vale en el editor y en el compilado.
static int LControls(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)W3dControlsCount());
    return 1;
}
static int LControl(lua_State* L) {
    int n = (int)luaL_optinteger(L, 1, 1) - 1;
    if (n < 0 || n >= W3dControlsCount()) { lua_pushnil(L); return 1; }
    lua_pushstring(L, W3dControlName(n));
    lua_pushstring(L, W3dControlType(n));
    return 2;
}

// importW3D("assets/pj/pj.w3d") -> ANEXA un .w3d (JSON v3 plano) a la escena
// EN RUNTIME: objetos + materiales + sus animaciones de escena (el streaming
// del modo juego; pedido del demo GTA). Devuelve el objeto RAIZ del archivo
// (light userdata, como object()), o nil. La ruta relativa cuelga del proyecto,
// mismo criterio que sound(). Los hijos del arbol anexado se agarran con
// find("nombre") del Core (object() solo resuelve refs pre-cableadas).
static int LImportW3D(lua_State* L) {
    const char* r = luaL_checkstring(L, 1);
    extern Object* W3dImportW3DAppendix(const std::string&);   // main/importers/import_w3d.h
    std::string path = r ? r : "";
    if (path.empty()) { lua_pushnil(L); return 1; }
    // (un script de una LIBRERIA externa: primero la de SU libreria, W3dScriptLibPath)
    const std::string fromLib = W3dScriptLibPath(L, path);
    const bool absolute = !fromLib.empty() || (path[0] == '/') || (path.size() > 2 && path[1] == ':');
    if (!fromLib.empty()) path = fromLib;
    if (!absolute && !g_w3dDirProject.empty()) path = g_w3dDirProject + "/" + path;
    Object* o = W3dImportW3DAppendix(path);
    if (o) lua_pushlightuserdata(L, o); else lua_pushnil(L);
    return 1;
}

// ---------------------------------------------------------------------------
//  REGISTRO + ALIMENTACION.
// ---------------------------------------------------------------------------
void BindsGameRegister(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    W3dPhysicsSetAdapter2D(PhysicsCanvas, PhysicsSize2D, PhysicsBoxUI,
                            PhysicsPosRaw);   // idempotente (se llama por cada lua_State)
    lua_pushcfunction(L, LScreen);    lua_setglobal(L, "screen");
    lua_pushcfunction(L, LMarginScreen); lua_setglobal(L, "screenMargin");
    lua_pushcfunction(L, LFrameScreen); lua_setglobal(L, "screenFrame");
    lua_pushcfunction(L, LCaptureScreen); lua_setglobal(L, "captureScreen");
    lua_pushcfunction(L, LCaptureUV);   lua_setglobal(L, "captureUV");
    lua_pushcfunction(L, LResolution);  lua_setglobal(L, "resolution");
    lua_pushcfunction(L, LSetMemory);  lua_setglobal(L, "setMemory");
    lua_pushcfunction(L, LMemory);     lua_setglobal(L, "getMemory");
    lua_pushcfunction(L, LSetMemories); lua_setglobal(L, "setMemories");
    lua_pushcfunction(L, LStep);        lua_setglobal(L, "step");      // un paso de rutina (adentro de "Llamar Lua")
    lua_pushcfunction(L, LSetStep);     lua_setglobal(L, "setStep");   // cambiar un paso de una rutina (por su indice)
    lua_pushcfunction(L, LReadStep);    lua_setglobal(L, "getStep");
    lua_pushcfunction(L, LPosPx);       lua_setglobal(L, "getPosPx");
    lua_pushcfunction(L, LSetPosPx);    lua_setglobal(L, "setPosPx");
    lua_pushcfunction(L, LSizePx);       lua_setglobal(L, "getSizePx");
    lua_pushcfunction(L, LSetSizePx);    lua_setglobal(L, "setSizePx");
    lua_pushcfunction(L, LSetText);    lua_setglobal(L, "setText");
    lua_pushcfunction(L, LSetTexture);  lua_setglobal(L, "setTexture");
    lua_pushcfunction(L, LDrawn);    lua_setglobal(L, "isDrawn");
    lua_pushcfunction(L, LSetFlipbook); lua_setglobal(L, "setFlipbook");
    lua_pushcfunction(L, LSetUVRect);   lua_setglobal(L, "setUVRect");
    lua_pushcfunction(L, LSound);      lua_setglobal(L, "sound");
    lua_pushcfunction(L, LStopSound); lua_setglobal(L, "stopSound");
    lua_pushcfunction(L, LReleaseSound); lua_setglobal(L, "releaseSound");
    lua_pushcfunction(L, LReverb);      lua_setglobal(L, "reverb");
    lua_pushcfunction(L, LClockAudio);  lua_setglobal(L, "audioClock");
    lua_pushcfunction(L, LClockMs);     lua_setglobal(L, "clockMs");
    lua_pushcfunction(L, LProfileLua);   lua_setglobal(L, "luaProfile");
    lua_pushcfunction(L, LLineCreate);  lua_setglobal(L, "createLine");
    lua_pushcfunction(L, LLinePoint);  lua_setglobal(L, "linePoint");
    lua_pushcfunction(L, LStopSounds); lua_setglobal(L, "stopSounds");
    lua_pushcfunction(L, LFitSound); lua_setglobal(L, "adjustSound");
    lua_pushcfunction(L, LSoundPosition); lua_setglobal(L, "soundPosition");
    lua_pushcfunction(L, LSetSector);   lua_setglobal(L, "setSector");
    lua_pushcfunction(L, LVisCell);      lua_setglobal(L, "visCell");
    lua_pushcfunction(L, LSetVisAnchor);   lua_setglobal(L, "setVisAnchor");
    lua_pushcfunction(L, LSetVisCurveT);  lua_setglobal(L, "setVisCurveT");
    lua_pushcfunction(L, LSetBatch);       lua_setglobal(L, "setBatch");
    lua_pushcfunction(L, LEmit);      lua_setglobal(L, "emit");
    lua_pushcfunction(L, LSetEmitting); lua_setglobal(L, "setEmitting");
    lua_pushcfunction(L, LFog);      lua_setglobal(L, "getFog");
    lua_pushcfunction(L, LSetFog);   lua_setglobal(L, "setFog");
    if (W3dSoundLuaHook) W3dSoundLuaHook(L);   // playSound / muteSound / soundObject / setSoundObject / createSound
    lua_pushcfunction(L, LShow);     lua_setglobal(L, "show");
    lua_pushcfunction(L, LSetTextureB);      lua_setglobal(L, "setTextureB");       // mezcla de texturas (material)
    lua_pushcfunction(L, LSetMixTexture); lua_setglobal(L, "setTextureMix");
    lua_pushcfunction(L, LMixTexture);    lua_setglobal(L, "getTextureMix");
    lua_pushcfunction(L, LSetMaterialUV);       lua_setglobal(L, "setMaterialUV");        // transformacion UV (material)
    lua_pushcfunction(L, LMaterialUV);          lua_setglobal(L, "getMaterialUV");
    lua_pushcfunction(L, LSetEmissionStrength); lua_setglobal(L, "setEmissionStrength");  // fuerza de la emision
    lua_pushcfunction(L, LEmissionStrength);    lua_setglobal(L, "getEmissionStrength");
    lua_pushcfunction(L, LSetOpacity); lua_setglobal(L, "setOpacity");
    lua_pushcfunction(L, LSetTint);    lua_setglobal(L, "setTint");
    lua_pushcfunction(L, LSetRot2D);    lua_setglobal(L, "setRot2D");
    lua_pushcfunction(L, LSetSize);      lua_setglobal(L, "setSize");
    lua_pushcfunction(L, LExit);       lua_setglobal(L, "quit");
    // facilidades: logica generica del juego (una linea en lua)
    lua_pushcfunction(L, LPressed);    lua_setglobal(L, "pressed");
    lua_pushcfunction(L, LCollide);      lua_setglobal(L, "collide");
    lua_pushcfunction(L, LLimit);     lua_setglobal(L, "limit");
    lua_pushcfunction(L, LInside);      lua_setglobal(L, "inside");
    lua_pushcfunction(L, LScale);      lua_setglobal(L, "getScale");
    lua_pushcfunction(L, LSafeArea);  lua_setglobal(L, "safeArea");
    lua_pushcfunction(L, LMerge);      lua_setglobal(L, "fade");
    lua_pushcfunction(L, LBoxUI);      lua_setglobal(L, "uiBox");
    lua_pushcfunction(L, LScreenOf);  lua_setglobal(L, "screenOf");
    lua_pushcfunction(L, LScreenPoint); lua_setglobal(L, "screenPoint");
    lua_pushcfunction(L, LRayScreen); lua_setglobal(L, "screenRay");
    // el RIEL de la camara, cambiable en juego (area abierta / elevador / sala lateral)
    lua_pushcfunction(L, LSetRail);     lua_setglobal(L, "setRail");
    lua_pushcfunction(L, LSetRailNode); lua_setglobal(L, "setRailNode");
    lua_pushcfunction(L, LSetLookRail); lua_setglobal(L, "setRailLook");
    lua_pushcfunction(L, LRailOf);      lua_setglobal(L, "getRail");
    lua_pushcfunction(L, LLensOf);     lua_setglobal(L, "getLens");
    lua_pushcfunction(L, LSetActiveCamera); lua_setglobal(L, "setActiveCamera");
    lua_pushcfunction(L, LSetLens);    lua_setglobal(L, "setLens");
    // los mandos enchufados (hotplug): cuantos hay y como se llama cada uno
    lua_pushcfunction(L, LControls);   lua_setglobal(L, "controls");
    lua_pushcfunction(L, LControl);     lua_setglobal(L, "control");
    // los tres que estaban SOLO en el editor (ver el bloque de arriba)
    lua_pushcfunction(L, LCameraXZ);    lua_setglobal(L, "cameraXZ");
    lua_pushcfunction(L, LTarget);    lua_setglobal(L, "target");
    lua_pushcfunction(L, LParameter);   lua_setglobal(L, "param");
    // streaming del modo juego: anexar un .w3d externo en caliente
    lua_pushcfunction(L, LImportW3D); lua_setglobal(L, "importW3D");
    // multi-escena: changeScene(nombre[,reiniciar]) — mismo bind en editor y runtime
    W3dSceneRegisterBind(Lv);
}

void BindsGameRender(float esc, float x0, float y0,
                      float safeWinX, float safeWinY, float safeWinW, float safeWinH,
                      const std::vector<UI2DPos>* positions) {
    gLastEsc = esc; gLastX0 = x0; gLastY0 = y0;
    gSafeWinX = safeWinX; gSafeWinY = safeWinY; gSafeWinW = safeWinW; gSafeWinH = safeWinH;
    gLastPos.clear();
    if (positions) gLastPos = *positions;
}

void BindsGamePoint(int slot, float cx, float cy, bool active) {
    if (slot < 0 || slot >= W3D_GAME_MAXDEDOS) return;
    gPunX[slot] = cx; gPunY[slot] = cy; gPunAct[slot] = active;
}

void BindsGameMouse(float cx, float cy, bool present) {
    gMouseCX = cx; gMouseCY = cy; gMousePresent = present;
}

bool BindsGameMouseState(float* cx, float* cy) {
    if (cx) *cx = gMouseCX;
    if (cy) *cy = gMouseCY;
    return gMousePresent;
}

void BindsGameSnapshotPointers(void) {
    // el flanco de pressed() del PROXIMO frame compara contra ESTE estado; se hace DESPUES de correr
    // los scripts para que todas las llamadas de ESTE frame vean el mismo "estado anterior".
    for (int i = 0; i < W3D_GAME_MAXDEDOS; i++) gPunPrev[i] = gPunAct[i];
    // (una vez por cuadro del juego, en el Play del editor y en el compilado: los objetos Sonido)
    if (W3dSoundsGameHook) W3dSoundsGameHook();
}

void BindsGameResetPointers(void) {
    for (int i = 0; i < W3D_GAME_MAXDEDOS; i++) {
        gPunAct[i] = false; gPunPrev[i] = false; gPunX[i] = 0.0f; gPunY[i] = 0.0f;
    }
    // al parar/reiniciar tambien se olvida el mouse: sin hover colgado de una corrida vieja
    gMousePresent = false; gMouseCX = 0.0f; gMouseCY = 0.0f;
}

// ---------------------------------------------------------------------------
//  LOS CONTROLES CONECTADOS (ver BindsGame.h): el registro compartido.
//  Aca NO entra SDL: quien maneja los eventos (el editor en main.cpp, el juego
//  compilado en su main generado) abre/cierra el device y llama a estas dos; el
//  aviso (toast + consola) sale igual en los dos lados porque se emite ACA, una
//  sola vez, en vez de estar copiado en cada plataforma.
// ---------------------------------------------------------------------------
struct W3dControlReg {
    void*       handle;
    int         id;
    std::string name;
    std::string type;
};
static std::vector<W3dControlReg> gControls;

int W3dControlsCount(void) { return (int)gControls.size(); }
const char* W3dControlName(int i) {
    return (i >= 0 && i < (int)gControls.size()) ? gControls[(size_t)i].name.c_str() : "";
}
const char* W3dControlType(int i) {
    return (i >= 0 && i < (int)gControls.size()) ? gControls[(size_t)i].type.c_str() : "";
}
void* W3dControlHandle(int i) {
    return (i >= 0 && i < (int)gControls.size()) ? gControls[(size_t)i].handle : 0;
}
int W3dControlIdIn(int i) {
    return (i >= 0 && i < (int)gControls.size()) ? gControls[(size_t)i].id : -1;
}

bool W3dControlAdd(void* handle, int id, const char* name, const char* type) {
    for (size_t i = 0; i < gControls.size(); i++)
        if (gControls[i].id == id) return false;      // ya estaba (evento repetido)
    W3dControlReg r;
    r.handle = handle;
    r.id     = id;
    r.name   = (name && *name) ? name : "Control";
    r.type   = (type && *type) ? type : "generic";
    gControls.push_back(r);
    w3dLogf("[control] se detecto un nuevo control: %s (%s), %d conectado(s)",
            r.name.c_str(), r.type.c_str(), (int)gControls.size());
    W3dAvisof(false, T("New controller detected: %s"), W3dShortName(r.name).c_str());
    return true;
}

bool W3dControlRemove(int id, void** handle) {
    if (handle) *handle = 0;
    for (size_t i = 0; i < gControls.size(); i++) {
        if (gControls[i].id != id) continue;
        std::string name = gControls[i].name;
        if (handle) *handle = gControls[i].handle;
        gControls.erase(gControls.begin() + (long)i);
        w3dLogf("[control] se desconecto un control: %s, quedan %d",
                name.c_str(), (int)gControls.size());
        W3dAvisof(true, T("Controller disconnected: %s"), W3dShortName(name).c_str());
        return true;
    }
    return false;
}

bool BindsGameWantsStop(void) { return gQuit; }
// mismo pedido, pero desde el RUNTIME (no desde lua): lo usa la captura de
// verificacion del juego compilado, que guarda un frame a PNG y cierra.
void BindsGameRequestExit(void) { gQuit = true; }
