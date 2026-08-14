// ============================================================================
//  BindsJuego.cpp — ver BindsJuego.h. Los binds 2D de juego COMPARTIDOS por el
//  editor (Play) y el runtime compilado, mas el estado por-frame que necesitan.
//  C++03 puro (Symbian/Android). Comentarios sin acentos.
// ============================================================================
#include "script/BindsJuego.h"
#include "script/W3dScript.h"          // W3dScriptParamObjeto
#include "physics/W3dFisica.h"         // adaptador 2D de la fisica del Core (lienzo + tamano del elemento)
#include "W3dEscena.h"                 // cambiarEscena(): el bind multi-escena COMPARTIDO
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Texto2D.h"
#include "objects/Boton2D.h"
#include "objects/Imagen2D.h"
#include "objects/Elemento2D.h"
#include "render/UIOverlay.h"          // UI2D_TamanoLienzo, UI2D_EsElemento2D, UI2DPos
#include "render/OpcionesRender.h"     // g_redraw (+ g_renderAspect para pantallaDe)
// (Camera.h NO se incluye aca: arrastra variables.h, que es solo del editor. Todo
//  lo que necesita una camara pasa por los hooks de BindsJuego.h, que instala quien
//  linkee el objeto Camera. Asi el bind existe y funciona en cualquier build que
//  traiga camara, en vez de ser un stub mudo segun el #ifdef del build.)
#include "io/UI2DFormato.h"            // g_w3dDirProyecto: base de las rutas relativas de sonido()
#include "W3dAlmacen.h"                // W3dAlmacenMontado(): setTextura respeta las entradas del contenedor v4
#include "base/W3dConfig.h"            // ConfigMudo(): mute global del Core
#include "W3dAviso.h"                  // W3dAvisof: el aviso de control conectado/desconectado

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <math.h>   // fabsf() para chocan()
#include <map>      // cache de sonidos cargados por sonido()
#include <string>

// audio del Core, forward-decl (evita el include del dir de audio, como en W3dScript.cpp).
// Sin -DW3D_ENABLE_AUDIO son stubs no-op -> sonido() no hace nada y no rompe.
namespace w3dEngine { class W3dSound; W3dSound* W3dSoundLoad(const char*); int W3dSoundPlayPitch(W3dSound*, float, bool, float); void W3dSoundStopFade(int, float); }

// ---------------------------------------------------------------------------
//  TAMANO del lienzo del juego: el MISMO que ve el render del overlay
//  (UI2D_TamanoLienzo). En el editor es el UI que se edita; en el runtime es
//  g_gameRenderW/H (que W3dGameRender deja igual al lienzo). Asi el script
//  (pantalla/posPx) y el dibujo COINCIDEN en los dos lados.
// ---------------------------------------------------------------------------
static void TamLienzo(float* w, float* h) {
    UI2D_TamanoLienzo(w, h);
    if (*w < 1.0f) *w = 1.0f;
    if (*h < 1.0f) *h = 1.0f;
}

// ---------------------------------------------------------------------------
//  ESTADO por-frame (lo alimentan los dos builds; ver BindsJuego.h).
// ---------------------------------------------------------------------------
// mapeo del ultimo render (ventana -> lienzo): escala + offset del letterbox / zona segura.
// OJO: solo valido DESPUES del primer render (arranca en 1,0,0) -> no en inicio().
static float gUltEsc = 1.0f, gUltX0 = 0.0f, gUltY0 = 0.0f;
// rect SEGURO (sin notch/barras) en px de VENTANA, tal como lo uso el ultimo render.
static float gSafeWinX = 0.0f, gSafeWinY = 0.0f, gSafeWinW = 0.0f, gSafeWinH = 0.0f;
// posiciones RESUELTAS por el layout (anclas/flex) en el ULTIMO render: para que
// cajaUI() lea el rect ya resuelto de un objeto anclado sin recalcularlo.
static std::vector<UI2DPos> gUltPos;

// PUNTEROS en coords de LIENZO (centro 0,0), las MISMAS que posPx/tamPx, mas el estado
// apretado ACTUAL y el del frame ANTERIOR (para el flanco de bajada de apretado()). Los
// slots son los dedos 0..3; el clic del mouse entra por el slot 0 (dedo 0 / toque()).
#define W3D_GAME_MAXDEDOS 4
static float gPunX[W3D_GAME_MAXDEDOS]    = {0,0,0,0};
static float gPunY[W3D_GAME_MAXDEDOS]    = {0,0,0,0};
static bool  gPunAct[W3D_GAME_MAXDEDOS]  = {false,false,false,false};
static bool  gPunPrev[W3D_GAME_MAXDEDOS] = {false,false,false,false};

// MOUSE-OVER en coords de LIENZO (centro 0,0): solo lo alimentan los eventos de MOUSE
// (runtime W3dGameRaton / editor SimRatonPantalla). El tactil NO pasa por aca, asi que
// gRatonPresente false = sin mouse = sin hover (el borde de hover no se dibuja).
static float gRatonCX = 0.0f, gRatonCY = 0.0f;
static bool  gRatonPresente = false;

// pedido de cierre del juego (lua salir()).
static bool gQuit = false;

// ---------------------------------------------------------------------------
//  Helpers de rect.
// ---------------------------------------------------------------------------
// tamano DECLARADO del elemento en px de LIENZO segun su unidad (tamModo): fraccion
// del lienzo, px directos, o px ESCALADOS (valor * min(w,h)/480, el factor del bind
// escala()). El MISMO tamano que resuelve el dibujo (render/UIOverlay.cpp).
static void TamDeclarado(Elemento2D* e, float w, float h, float* aw, float* ah) {
    if (e->tamModo == TAM2D_PX) { *aw = e->ancho; *ah = e->alto; return; }
    if (e->tamModo == TAM2D_ESCALADO) {
        float m = (w < h) ? w : h;
        float f = (m >= 1.0f) ? m / 480.0f : 1.0f;
        *aw = e->ancho * f; *ah = e->alto * f;
        return;
    }
    *aw = e->ancho * w; *ah = e->alto * h;
}
// rect del objeto en coords de LIENZO (centro 0,0): centro (cx,cy) + medias extensiones
// (hw,hh). Respeta la unidad del tamano (tamModo). false si el objeto es nil.
static bool RectLienzo(Object* o, float* cx, float* cy, float* hw, float* hh) {
    if (!o) return false;
    float w, h; TamLienzo(&w, &h);
    *cx = o->pos.x * w; *cy = o->pos.y * h;
    float aw = 0.0f, ah = 0.0f;
    if (UI2D_EsElemento2D(o)) TamDeclarado((Elemento2D*)o, w, h, &aw, &ah);
    *hw = aw * 0.5f; *hh = ah * 0.5f;
    return true;
}
// busca el rect que OCUPO el objeto 'o' en el ULTIMO render y lo pasa a coords de LIENZO
// (centro 0,0), con (cx,cy) = CENTRO y (aw,ah) = tamano. false si no se dibujo.
static bool RectResueltoLienzo(Object* o, float* cx, float* cy, float* aw, float* ah) {
    if (!o || gUltEsc <= 0.0001f) return false;
    float lw, lh; TamLienzo(&lw, &lh);
    for (size_t i = 0; i < gUltPos.size(); i++) {
        if (gUltPos[i].obj != o) continue;
        const UI2DPos& p = gUltPos[i];
        float mx = (p.bx0 + p.bx1) * 0.5f, my = (p.by0 + p.by1) * 0.5f;   // centro en px de ventana
        *cx = (mx - gUltX0) / gUltEsc - lw * 0.5f;   // ventana -> lienzo
        *cy = (my - gUltY0) / gUltEsc - lh * 0.5f;
        *aw = (p.bx1 - p.bx0) / gUltEsc;
        *ah = (p.by1 - p.by0) / gUltEsc;
        return true;
    }
    return false;
}
// CAJA de un objeto en coords de LIENZO (centro + tamano): el rect RESUELTO por el layout si ya se
// dibujo, y si no la pos/tam crudos. Es lo que devuelve cajaUI() y lo que usan limitar(o, area),
// dentro(area, x, y) y la fisica cuando un objeto hace de area: un solo lugar, una sola respuesta.
static bool CajaLienzo(Object* o, float* cx, float* cy, float* aw, float* ah) {
    if (!o) return false;
    if (RectResueltoLienzo(o, cx, cy, aw, ah)) return true;
    float w, h; TamLienzo(&w, &h);
    *cx = o->pos.x * w; *cy = o->pos.y * h; *aw = 0.0f; *ah = 0.0f;
    if (UI2D_EsElemento2D(o)) TamDeclarado((Elemento2D*)o, w, h, aw, ah);
    return true;
}
// ---------------------------------------------------------------------------
//  REGLA CRUDO-vs-RESUELTO (colisiones: chocan/limitar y la fisica del Core).
//  Hay dos "verdades" posibles para la posicion de un objeto 2D:
//    * la CRUDA (o->pos * lienzo): es del FRAME ACTUAL, pero solo coincide con
//      lo dibujado si el layout no lo mueve;
//    * la RESUELTA (gUltPos, el rect del ULTIMO render): siempre coincide con
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
// (render/UIOverlay.cpp, DibujarElemRec):
//   * Elemento2D hijo DIRECTO de la raiz UI: en los anidados la pos se guarda
//     RELATIVA al rect del padre (pos.x * anchoDelPadre), asi que pos * lienzo
//     no es su centro;
//   * la raiz en layout LIBRE (layoutHijos == 0): en filas/columnas manda la
//     celda y la pos se ignora;
//   * ancla CENTRO (ancla == 0): con ancla a borde/esquina la pos es un
//     corrimiento desde ese punto, no desde el centro del lienzo;
//   * la raiz SIN padding: el padding corre el rect de referencia del ancla.
static bool PosCrudaAutoritativa(Object* o) {
    if (!o || !UI2D_EsElemento2D(o)) return false;
    Object* p = o->Parent;
    if (!p || p->getType() != ObjectType::ui) return false;
    UI* u = (UI*)p;
    if (u->layoutHijos != 0) return false;
    if (((Elemento2D*)o)->ancla != 0) return false;
    if (u->padIzq != 0.0f || u->padDer != 0.0f ||
        u->padArr != 0.0f || u->padAba != 0.0f) return false;
    return true;
}
// CAJA DE COLISION de un operando de chocan()/limitar(): centro (cx,cy) +
// MEDIAS extensiones (hw,hh) aplicando la regla crudo-vs-resuelto:
//   * pos cruda AUTORITATIVA -> centro CRUDO del frame actual + tamano
//     DECLARADO (TamDeclarado respeta tamModo): sin lag, como siempre;
//   * si no -> CajaLienzo (rect RESUELTO del ultimo render, que es donde el
//     objeto se dibuja de verdad; con fallback al crudo si nunca se dibujo).
static bool CajaColision(Object* o, float* cx, float* cy, float* hw, float* hh) {
    if (!o) return false;
    if (PosCrudaAutoritativa(o)) return RectLienzo(o, cx, cy, hw, hh);
    float aw = 0.0f, ah = 0.0f;
    if (!CajaLienzo(o, cx, cy, &aw, &ah)) return false;
    *hw = aw * 0.5f; *hh = ah * 0.5f;
    return true;
}
// alto (en px de LIENZO) del rect de REFERENCIA del ancla del objeto en el
// ultimo render: la unidad en la que se guarda su pos.y (pos.y = fraccion de
// este alto). Lo usa limitar() para aplicar un delta a un objeto NO
// autoritativo sin teletransportarlo. false si el objeto no se dibujo.
static bool RefAltoLienzo(Object* o, float* refH) {
    if (!o || gUltEsc <= 0.0001f) return false;
    for (size_t i = 0; i < gUltPos.size(); i++) {
        if (gUltPos[i].obj != o) continue;
        if (gUltPos[i].refH <= 0.0001f) return false;
        *refH = gUltPos[i].refH / gUltEsc;   // px de ventana -> px de lienzo
        return true;
    }
    return false;
}
// ANCHO del rect de referencia, espejo exacto del de arriba: la unidad en la que
// se guarda pos.x. Lo necesita limitar() para poder recortar tambien en X.
static bool RefAnchoLienzo(Object* o, float* refW) {
    if (!o || gUltEsc <= 0.0001f) return false;
    for (size_t i = 0; i < gUltPos.size(); i++) {
        if (gUltPos[i].obj != o) continue;
        if (gUltPos[i].refW <= 0.0001f) return false;
        *refW = gUltPos[i].refW / gUltEsc;   // px de ventana -> px de lienzo
        return true;
    }
    return false;
}
// CAJA para el HIT-TEST de TOQUE (apretado): el rect RESUELTO por el layout (CajaLienzo,
// con su fallback al crudo si el objeto no se dibujo aun), ENSANCHADO al tamano DECLARADO
// del elemento (max por eje, centrado en el centro resuelto). El ensanche es porque para
// texto2d el rect resuelto que guarda el overlay es el BOUNDS medido de los glifos, mas
// chico que la caja ancho/alto del .w3dui: sin esto el area de toque de una opcion de menu
// se achicaria a las letras. Para boton2d/rect el resuelto ya es el rect dibujado y el max
// solo garantiza no tocar MENOS que antes. Devuelve MEDIAS extensiones (hw,hh) como RectLienzo.
static bool CajaHitLienzo(Object* o, float* cx, float* cy, float* hw, float* hh) {
    float aw = 0.0f, ah = 0.0f;
    if (!CajaLienzo(o, cx, cy, &aw, &ah)) return false;
    // ensanche SOLO para texto2d: su rect resuelto es el bounds MEDIDO de los glifos (mas chico
    // que la caja de toque declarada con ancho/alto) y sin esto el area de tap se achicaria.
    // Para boton/rect/contenedor el rect resuelto por el layout ES la caja real: usarla tal cual.
    // (Ensanchar con el tamano "declarado" en esos tipos rompia las grillas flex: los botones que
    // no declaran tamano heredan el default del constructor (200x150) y pisaban a los vecinos.)
    if (o->getType() == ObjectType::texto2d && UI2D_EsElemento2D(o)) {
        float w, h; TamLienzo(&w, &h);
        float dw, dh;   // tamano declarado en px de lienzo (respeta tamModo)
        TamDeclarado((Elemento2D*)o, w, h, &dw, &dh);
        if (dw > aw) aw = dw;
        if (dh > ah) ah = dh;
    }
    *hw = aw * 0.5f; *hh = ah * 0.5f;
    return true;
}

// ---------------------------------------------------------------------------
//  BINDS (coordenadas en px de lienzo, centro 0,0).
// ---------------------------------------------------------------------------
static int LPantalla(lua_State* L) {
    float w, h; TamLienzo(&w, &h);
    lua_pushnumber(L, w); lua_pushnumber(L, h); return 2;
}
static int LPosPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (!o) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    lua_pushnumber(L, o->pos.x * w); lua_pushnumber(L, o->pos.y * h); return 2;
}
// setPosPx(o, x, y): pone el objeto ahi. Un eje en NIL se DEJA COMO ESTA:
//   setPosPx(pala, nil, dy) mueve la pala solo en Y (no hay que leer la X para conservarla)
//   setPosPx(o, x, nil) al reves. setPosPx(o, nil, nil) no hace nada.
static int LSetPosPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (o && w > 0.0f && h > 0.0f) {
        if (!lua_isnoneornil(L, 2)) o->pos.x = (float)luaL_checknumber(L, 2) / w;
        if (!lua_isnoneornil(L, 3)) o->pos.y = (float)luaL_checknumber(L, 3) / h;
        g_redraw = true;
    }
    return 0;
}
static int LTamPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (o && UI2D_EsElemento2D(o)) {
        float aw, ah;
        TamDeclarado((Elemento2D*)o, w, h, &aw, &ah);
        lua_pushnumber(L, aw);
        lua_pushnumber(L, ah);
    } else { lua_pushnumber(L, 0); lua_pushnumber(L, 0); }
    return 2;
}
static int LSetTamPx(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    if (o && UI2D_EsElemento2D(o) && w > 0.0f && h > 0.0f) {
        Elemento2D* e = (Elemento2D*)o;
        float nw = (float)luaL_checknumber(L, 2), nh = (float)luaL_checknumber(L, 3);
        // se recibe en px de lienzo y se guarda EN LA UNIDAD del elemento
        if (e->tamModo == TAM2D_PX) { e->ancho = nw; e->alto = nh; }
        else if (e->tamModo == TAM2D_ESCALADO) {
            float m = (w < h) ? w : h;
            float f = (m >= 1.0f) ? m / 480.0f : 1.0f;
            e->ancho = nw / f; e->alto = nh / f;
        }
        else { e->ancho = nw / w; e->alto = nh / h; }
        g_redraw = true;
    }
    return 0;
}
static int LSetTexto(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (!o) return 0;
    const char* s = lua_tostring(L, 2);   // numeros se convierten solos
    if (o->getType() == ObjectType::texto2d) {
        ((Texto2D*)o)->texto = s ? s : "";
        g_redraw = true;
    } else if (o->getType() == ObjectType::boton2d) {
        // el rotulo de un BOTON tambien se cambia por script (idiomas del menu)
        ((Boton2D*)o)->texto = s ? s : "";
        g_redraw = true;
    }
    return 0;
}
// setTextura(objeto, "ruta.png"): cambia la imagen de un objeto tipo imagen (prender/apagar el
// parlante, el coleccionable del HUD girando). La ruta RELATIVA se resuelve como en sonido(): cuelga de
// la carpeta del PROYECTO (g_w3dDirProyecto). Las rutas del .w3dui se resuelven al CARGAR
// (UI2DFormato::RutaAlCargar), pero las que llegan por script entraban CRUDAS y caian al cwd:
// ninguna textura. Si hay contenedor v4 montado y la ruta es una ENTRADA suya, queda tal cual
// (ReadFileBytes la resuelve por el montaje, mismo contrato que el resto de los assets).
// El decode ya se cachea POR RUTA (Textura2DObtener, fallos incluidos): ciclar 14 PNG de una
// animacion decodifica cada una UNA vez y despues es solo un bind.
static int LSetTextura(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    const char* s = lua_tostring(L, 2);
    if (o && o->getType() == ObjectType::imagen2d && s) {
        std::string res = s;
        bool absoluta = !res.empty() && (res[0] == '/' || (res.size() > 1 && res[1] == ':'));
        if (!absoluta && !g_w3dDirProyecto.empty()) {
            W3dAlmacen* alm = W3dAlmacenMontado();
            if (!(alm && alm->Existe(res)))
                res = g_w3dDirProyecto + "/" + res;
        }
        ((Imagen2D*)o)->textura = res;
        g_redraw = true;
    }
    return 0;
}
// sonido("sonidos/bip.wav" [, vol [, pitch]]): reproduce un WAV one-shot. La ruta relativa
// cuelga de la carpeta del PROYECTO (g_w3dDirProyecto, la del .w3d abierto); vol 0..1 (default 1);
// pitch 1 = normal (paso fraccional en la voz, W3dSoundPlayPitch). El WAV se carga UNA vez y se
// cachea por ruta resuelta (NULL tambien: un archivo que falta no reintenta cada frame). No
// bloquea: si el mixer no abrio (headless / sin -DW3D_ENABLE_AUDIO) la carga da NULL y no suena.
static int LSonido(lua_State* L) {
    static std::map<std::string, w3dEngine::W3dSound*> gSonidos;   // cache por ruta resuelta (NULL tambien se cachea)
    const char* ruta = luaL_checkstring(L, 1);
    float vol   = (float)luaL_optnumber(L, 2, 1.0);
    float pitch = (float)luaL_optnumber(L, 3, 1.0);
    bool  loop  = lua_toboolean(L, 4) != 0;   // arg 4 opcional: true = sample en LOOP (nota sostenida)
    if (vol < 0.0f) vol = 0.0f; if (vol > 1.0f) vol = 1.0f;
    if (w3dEngine::ConfigMudo()) return 0;   // mute global del Core (mismo criterio que beep()); nil = no sono
    // resolver la ruta: absoluta queda tal cual; relativa cuelga del proyecto (patron UI2DFormato)
    std::string res = ruta;
    if (!(res.size() > 0 && (res[0] == '/' || (res.size() > 1 && res[1] == ':'))))
        if (!g_w3dDirProyecto.empty()) res = g_w3dDirProyecto + "/" + res;
    w3dEngine::W3dSound* s;
    std::map<std::string, w3dEngine::W3dSound*>::iterator it = gSonidos.find(res);
    if (it != gSonidos.end()) s = it->second;
    else { s = w3dEngine::W3dSoundLoad(res.c_str()); gSonidos[res] = s; }
    // DEVUELVE el handle de la voz (> 0) o nil si no sono (WAV faltante, mixer cerrado,
    // 32 voces ocupadas). El handle se le pasa a pararSonido(h [, fadeSeg]) — es lo que
    // necesita el secuenciador de musica en lua (notas sostenidas: loop=true + release).
    int id = s ? w3dEngine::W3dSoundPlayPitch(s, vol, loop, pitch) : 0;
    if (id <= 0) return 0;   // nil
    lua_pushinteger(L, id);
    return 1;
}
// pararSonido(handle [, fadeSeg]): corta la voz devuelta por sonido() con una RAMPA
// corta (default ~5 ms; el mixer la baja a cero por muestra -> no clickea). fadeSeg
// mas largo = release musical. handle nil/0/viejo = no-op seguro (los ids de voz no
// se reciclan: cortar una voz que ya termino no toca a la que reuso el slot).
static int LPararSonido(lua_State* L) {
    int id = (int)luaL_optinteger(L, 1, 0);
    float fade = (float)luaL_optnumber(L, 2, 0.005);
    if (id > 0) w3dEngine::W3dSoundStopFade(id, fade);
    return 0;
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
    Object* o = W3dScriptParamObjeto(L, 1);
    int sec = (int)luaL_optinteger(L, 2, 0);
    if (o && W3dPVSSetSectorHook) W3dPVSSetSectorHook(o, sec);
    return 0;
}
// --- VISIBILIDAD POR CELDA (VisSet / VisZona) ---
// visCelda(obj): la celda ACTIVA (1-based; 0 = malla completa). obj puede ser la
// malla con el modificador Culling por triangulo o una VisZona. nil = sin dato.
// setVisAncla(zona, obj): fija el objeto cuya posicion decide la celda en los
// modos grilla/volumenes de la zona (pisa el ancla declarada en el .w3d).
// setVisCurvaT(zona, t [, riel]): modo curva: el juego entrega el indice
// FRACCIONARIO del nodo del riel (el mismo ancla que ya calcula para la camara);
// la zona avanza de a UNA celda por tick hacia ese objetivo (el stepper vive en
// la zona). El 3er argumento (opcional) identifica DE QUE riel es ese indice:
// un string con el nombre de la Curve, o la Curve misma. Si la zona declara
// `riel:` y no coincide, cae a su fallback (multi-riel, ver VisZona.h); sin
// declarar riel el identificador se ignora (compatibilidad total).
// Mismo patron de HOOKS que setSector: el editor los instala (MeshEdit/VisZona);
// sin instalar, los binds son no-op seguros que linkean igual.
int  (*W3dVisCeldaHook)(Object*) = 0;
bool (*W3dVisAnclaHook)(Object*, Object*) = 0;
bool (*W3dVisCurvaTHook)(Object*, float, const char*) = 0;
static int LVisCelda(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (!o || !W3dVisCeldaHook) { lua_pushnil(L); return 1; }
    int c = W3dVisCeldaHook(o);
    if (c < 0) { lua_pushnil(L); return 1; }   // el objeto no participa del sistema
    lua_pushinteger(L, c);
    return 1;
}
static int LSetVisAncla(lua_State* L) {
    Object* zona = W3dScriptParamObjeto(L, 1);
    Object* ancla = W3dScriptParamObjeto(L, 2);
    if (zona && W3dVisAnclaHook) W3dVisAnclaHook(zona, ancla);
    return 0;
}
static int LSetVisCurvaT(lua_State* L) {
    Object* zona = W3dScriptParamObjeto(L, 1);
    float t = (float)luaL_optnumber(L, 2, 0.0);
    // riel (opcional): string con el nombre, o el objeto Curve (se usa su nombre)
    const char* riel = 0;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        if (lua_isstring(L, 3)) riel = lua_tostring(L, 3);
        else { Object* r = W3dScriptParamObjeto(L, 3); if (r) riel = r->name.c_str(); }
    }
    if (zona && W3dVisCurvaTHook) W3dVisCurvaTHook(zona, t, riel);
    return 0;
}
// setLote(coleccion, "estatico"|"off"): prende/apaga el lote estatico de una
// Collection (P4). Mismo patron de hook (lo instala Collection.cpp); no-op seguro.
bool (*W3dSetLoteHook)(Object*, const char*) = 0;
static int LSetLote(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    const char* modo = luaL_optstring(L, 2, "off");
    if (o && W3dSetLoteHook) W3dSetLoteHook(o, modo);
    return 0;
}
// emitir(obj, n): RAFAGA de n particulas del objeto Particulas YA (con cantidad: 0 el
// emisor emite SOLO por rafagas). Hook instalado por el editor (ver BindsJuego.h):
// en el runtime compilado queda NULL y el bind es un no-op seguro.
void (*W3dParticulasEmitirHook)(Object* o, int n) = 0;

// LA CAMARA POR HOOK (ver BindsJuego.h): los instala main/objects/Camera.cpp.
bool (*W3dCamaraProyectarHook)(Object*, float, float, float*, float*, bool*) = 0;
bool (*W3dCamaraRielSetHook)(Object*, Object*, const char*, bool, const int*) = 0;
bool (*W3dCamaraRielNodoHook)(Object*, float) = 0;
bool (*W3dCamaraRielMiradaHook)(Object*, bool) = 0;
bool (*W3dCamaraRielEstadoHook)(Object*, const char**, int*, float*, bool*, float*) = 0;
bool (*W3dCamaraLenteHook)(Object*, float*, float*, float*) = 0;
bool (*W3dCamaraSetLenteHook)(Object*, const float*, const float*, const float*) = 0;
bool (*W3dCamaraXZHook)(float*, float*, float*, float*) = 0;
Object* (*W3dObjetivoHook)(Object*) = 0;
static int LEmitir(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    int n = (int)luaL_optinteger(L, 2, 1);
    if (o && n > 0 && W3dParticulasEmitirHook) W3dParticulasEmitirHook(o, n);
    return 0;
}
// mostrar(obj, visible): prende/apaga el dibujado (para menus del juego).
// ALIAS historico de setVisible() (Core): hace exactamente lo mismo y se mantiene por los scripts
// que ya lo usan. El lector es visible(obj), que tambien vive en el Core.
static int LMostrar(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (o) { o->visible = lua_toboolean(L, 2) != 0; g_redraw = true; }
    return 0;
}
// setOpacidad(obj, 0..1): opacidad del elemento 2D (se hereda a los hijos -> crossfade de un menu).
static int LSetOpacidad(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (o && UI2D_EsElemento2D(o)) {
        float a = (float)luaL_checknumber(L, 2);
        if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
        ((Elemento2D*)o)->opacidad = a;
        g_redraw = true;
    }
    return 0;
}
// setTam(obj, px): tamano de fuente de un texto 2D.
static int LSetTam(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (o && o->getType() == ObjectType::texto2d) {
        ((Texto2D*)o)->tam = (float)luaL_checknumber(L, 2);
        g_redraw = true;
    }
    return 0;
}
// salir(): pide cerrar el juego (la plataforma corta el loop). En el editor no hace efecto visible.
static int LSalir(lua_State*) { gQuit = true; return 0; }

// apretado(objeto) -> bool. true SOLO en el frame del tap/click NUEVO (flanco de bajada) cuyo punto
// cae DENTRO del rect en pantalla del objeto. Vale para mouse Y para cualquier dedo (multitouch).
// El rect es el RESUELTO por el layout (anclas/flex, como cajaUI): un boton dentro de un contenedor
// se toca DONDE SE DIBUJA; si aun no se dibujo cae al rect crudo (CajaHitLienzo).
static int LApretado(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float cx, cy, hw, hh;
    if (!o || !CajaHitLienzo(o, &cx, &cy, &hw, &hh)) { lua_pushboolean(L, 0); return 1; }
    bool nuevo = false;
    for (int i = 0; i < W3D_GAME_MAXDEDOS; i++) {
        if (gPunAct[i] && !gPunPrev[i]) {   // recien apretado este frame
            float px = gPunX[i], py = gPunY[i];
            if (px >= cx - hw && px <= cx + hw && py >= cy - hh && py <= cy + hh) { nuevo = true; break; }
        }
    }
    lua_pushboolean(L, nuevo ? 1 : 0);
    return 1;
}
// chocan(a, b) -> bool. true si los rects EN PANTALLA de a y b se solapan (AABB).
// Cada operando usa la regla crudo-vs-resuelto (CajaColision): los objetos de
// gameplay (hijos directos de la raiz, ancla centro, layout libre) se comparan
// por su pos CRUDA de este frame (sin lag: un setPosPx del mismo frame ya
// cuenta), y los puestos por el layout (flex / ancla a borde) por su rect
// RESUELTO, que es donde se dibujan (antes se usaba el crudo para todos y
// chocan no coincidia con cajaUI ni con lo que se veia).
static int LChocan(lua_State* L) {
    Object* a = W3dScriptParamObjeto(L, 1);
    Object* b = W3dScriptParamObjeto(L, 2);
    float ax, ay, ahw, ahh, bx, by, bhw, bhh;
    if (!CajaColision(a, &ax, &ay, &ahw, &ahh) || !CajaColision(b, &bx, &by, &bhw, &bhh)) {
        lua_pushboolean(L, 0); return 1;
    }
    bool hit = fabsf(ax - bx) <= (ahw + bhw) && fabsf(ay - by) <= (ahh + bhh);
    lua_pushboolean(L, hit ? 1 : 0);
    return 1;
}
// limitar(objeto, area [, eje]): mueve el objeto para que su rect no se salga de la caja de
//   'area' (la que da cajaUI, o sea la YA resuelta por el layout). Es la forma recomendada:
//   el juego no calcula ni lee ningun borde, el .w3dui manda.
// limitar(objeto, min, max [, eje]): la misma idea con los dos bordes a mano (px de lienzo);
//   con eje "x" son izquierda/derecha y con "y" arriba/abajo.
//
// 'eje' es "xy" (default), "x" o "y" -- la misma convencion de nombres de eje que ya usa
// rebotarEn(obj, area, "arriba abajo") del Core. Antes esta funcion recortaba SOLO en Y: el
// nombre y la doc eran genericos ("que su rect no se salga") pero la semantica era "clamp
// vertical", heredada de la pala de un pong. Un objeto que se sale por un costado no tenia
// como acotarse.
// El objeto MOVIL usa la MISMA regla crudo-vs-resuelto que chocan (CajaColision):
//   * autoritativo (pala del pong): caja CRUDA de este frame -> el clamp del mismo
//     frame que el setPosPx, sin lag (identico a como era siempre);
//   * puesto por el layout (ancla a borde / dentro de un flex): caja RESUELTA (la
//     dibujada) y el ajuste se aplica como DELTA sobre su pos, en la fraccion de
//     su rect de referencia (RefAltoLienzo) -> se corre lo justo, sin teletransporte
//     (antes se le pisaba pos.y con un centro crudo que no era el suyo). En una
//     celda de filas/columnas la pos se ignora al dibujar: el delta no tiene efecto
//     visible (el layout manda; documentado a proposito).
static int LLimitar(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    float cx, cy, hw, hh;
    if (!o || !CajaColision(o, &cx, &cy, &hw, &hh) || h <= 0.0f) return 0;
    Object* area = W3dScriptParamObjeto(L, 2);
    // el eje es el ULTIMO argumento y es texto: con area va en 3, con dos bordes en 4
    const char* eje = luaL_optstring(L, area ? 3 : 4, "xy");
    bool enX = false, enY = false;
    for (const char* p = eje; p && *p; p++) { if (*p == 'x' || *p == 'X') enX = true;
                                             if (*p == 'y' || *p == 'Y') enY = true; }
    if (!enX && !enY) { enX = true; enY = true; }   // texto raro: el default
    float izq = 0, der = 0, arriba = 0, abajo = 0;
    if (area) {
        float acx, acy, aw, ah;
        if (!CajaLienzo(area, &acx, &acy, &aw, &ah) || ah <= 0.0f) return 0;
        izq    = acx - aw * 0.5f; der   = acx + aw * 0.5f;
        arriba = acy - ah * 0.5f; abajo = acy + ah * 0.5f;
    } else {
        // dos numeros: son los bordes del eje pedido. Con "xy" (el default) se
        // interpretan como arriba/abajo, que es lo que hacia siempre.
        const float a = (float)luaL_checknumber(L, 2);
        const float b = (float)luaL_checknumber(L, 3);
        if (enX && !enY) { izq = a; der = b; }
        else             { arriba = a; abajo = b; enY = true; enX = false; }
    }
    if (enY) {
        float nuevo = cy;
        if (nuevo - hh < arriba) nuevo = arriba + hh;   // no pasar el borde de arriba
        if (nuevo + hh > abajo)  nuevo = abajo  - hh;   // ni el de abajo
        if (PosCrudaAutoritativa(o)) {
            o->pos.y = nuevo / h;   // fraccion del lienzo, como setPosPx (camino de siempre)
        } else {
            float refH;
            if (!RefAltoLienzo(o, &refH)) refH = h;   // sin resuelto: cae al lienzo (como el crudo)
            o->pos.y += (nuevo - cy) / refH;          // delta en la unidad de SU rect de referencia
        }
    }
    if (enX && w > 0.0f) {
        float nuevo = cx;
        if (nuevo - hw < izq) nuevo = izq + hw;
        if (nuevo + hw > der) nuevo = der - hw;
        if (PosCrudaAutoritativa(o)) {
            o->pos.x = nuevo / w;
        } else {
            float refW;
            if (!RefAnchoLienzo(o, &refW)) refW = w;
            o->pos.x += (nuevo - cx) / refW;
        }
    }
    g_redraw = true;
    return 0;
}
// dentro(area, x, y) -> bool: true si el punto cae DENTRO de la caja del objeto 'area' (la de cajaUI,
//   ya resuelta por el layout). Un eje en NIL no se mira: dentro(cancha, x) pregunta solo si esta
//   entre los costados. Sirve para las reglas que se escriben contra una zona del layout ("el dedo
//   cayo en la cancha", "la pelota se fue por un costado") sin derivar ni un borde en el script.
static int LDentro(lua_State* L) {
    Object* area = W3dScriptParamObjeto(L, 1);
    float cx, cy, aw, ah;
    if (!area || !CajaLienzo(area, &cx, &cy, &aw, &ah)) { lua_pushboolean(L, 0); return 1; }
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
// escala() -> factor responsive = min(anchoPantalla, altoPantalla) / 480.
// escala(objeto) -> sx, sy, sz: la escala del OBJETO (el mismo bind del Core, script/W3dScript.cpp).
// Las dos formas conviven en un solo nombre porque este registro corre DESPUES del Core y lo pisa:
// si viene un objeto se responde la escala del objeto, si no el factor de pantalla de siempre.
static int LEscala(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (o) {
        lua_pushnumber(L, o->scale.x); lua_pushnumber(L, o->scale.y); lua_pushnumber(L, o->scale.z);
        return 3;
    }
    float w, h; TamLienzo(&w, &h);
    float m = (w < h) ? w : h;
    lua_pushnumber(L, m / 480.0f);
    return 1;
}
// areaSegura() -> x, y, ancho, alto: el rect seguro (sin notch/barras) en coords de LIENZO, con (x,y)
// en la ESQUINA superior-izquierda. Antes del primer render (o sin zona segura) cae al lienzo entero.
static int LAreaSegura(lua_State* L) {
    float lw, lh; TamLienzo(&lw, &lh);
    float x = -lw * 0.5f, y = -lh * 0.5f, aw = lw, ah = lh;
    if (gUltEsc > 0.0001f && gSafeWinW > 0.0f && gSafeWinH > 0.0f) {
        x  = (gSafeWinX - gUltX0) / gUltEsc - lw * 0.5f;
        y  = (gSafeWinY - gUltY0) / gUltEsc - lh * 0.5f;
        aw = gSafeWinW / gUltEsc;
        ah = gSafeWinH / gUltEsc;
    }
    lua_pushnumber(L, x); lua_pushnumber(L, y);
    lua_pushnumber(L, aw); lua_pushnumber(L, ah);
    return 4;
}
// fundir(objeto, objetivo, paso): acerca la opacidad del objeto hacia objetivo (0..1) a lo sumo 'paso'.
static int LFundir(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    if (o && UI2D_EsElemento2D(o)) {
        float obj  = (float)luaL_checknumber(L, 2);
        float paso = (float)luaL_checknumber(L, 3);
        if (obj < 0.0f) obj = 0.0f; if (obj > 1.0f) obj = 1.0f;
        if (paso < 0.0f) paso = -paso;
        Elemento2D* e = (Elemento2D*)o;
        float a = e->opacidad;
        if (a < obj)      { a += paso; if (a > obj) a = obj; }
        else if (a > obj) { a -= paso; if (a < obj) a = obj; }
        e->opacidad = a;
        g_redraw = true;
    }
    return 0;
}
// cajaUI(objeto) -> x, y, ancho, alto: el rect YA RESUELTO por el layout (anclas/flexbox) en coords de
// LIENZO, con (x,y) = CENTRO (como posPx) y (ancho,alto) = tamano (como tamPx). Si el objeto no se dibujo
// aun (o esta oculto) cae a la pos/tam crudos, como posPx/tamPx.
static int LCajaUI(lua_State* L) {
    float cx = 0.0f, cy = 0.0f, aw = 0.0f, ah = 0.0f;
    CajaLienzo(W3dScriptParamObjeto(L, 1), &cx, &cy, &aw, &ah);   // false (nil) -> ceros
    lua_pushnumber(L, cx); lua_pushnumber(L, cy);
    lua_pushnumber(L, aw); lua_pushnumber(L, ah);
    return 4;
}

// pantallaDe(obj) -> x, y, delante: la posicion 3D del objeto PROYECTADA al lienzo
// de la UI, en PIXELES con centro en (0,0) — EXACTAMENTE el espacio de posPx/
// setPosPx (x a la derecha, y hacia ABAJO): lo que devuelve se le pasa TAL CUAL a
// setPosPx para que un elemento 2D siga a un objeto 3D (la fruta que vuela hasta
// el contador del HUD: setPosPx(icono, pantallaDe(fruta))). El tercer retorno
// 'delante' = true si el objeto esta DELANTE de la camara; con false x/y son la
// proyeccion espejada de atras (ocultá el elemento en ese caso).
// Usa la camara ACTIVA del juego (CameraActive) con SU lente y el aspecto del
// render (g_renderAspect, los mismos numeros del modo juego; antes del primer
// render cae al aspecto del lienzo). Sin camara activa devuelve 0, 0, false.
// OJO: con camara ORTOGRAFICA proyecta con la lente en perspectiva (aproximado).
static int LPantallaDe(lua_State* L) {
    Object* o = W3dScriptParamObjeto(L, 1);
    float w, h; TamLienzo(&w, &h);
    float sx = 0, sy = 0; bool delante = false;
    if (!o || !W3dCamaraProyectarHook || !W3dCamaraProyectarHook(o, w, h, &sx, &sy, &delante)) {
        lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushboolean(L, 0);
        return 3;
    }
    lua_pushnumber(L, sx);
    lua_pushnumber(L, sy);
    lua_pushboolean(L, delante ? 1 : 0);
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
//  Camera::SetRielObjeto).
//
//    setRiel(camara, riel [, offsetRiel])   riel = objeto Curve | "Nombre" | nil
//    setRielNodo(camara, indice)            indice >= 0 fija el nodo; -1 = seguir al target
//    setMiradaRiel(camara, on)              on = mirar con el yaw/pitch autoral del nodo
//    rielDe(camara) -> nombre, offsetRiel, nodo, miradaRiel, indice
// ---------------------------------------------------------------------------
// (los helpers CamaraArg / CurvaArg vivian aca y tenian que conocer Camera y Curve,
//  de ahi el #ifdef. Ahora el tipo lo verifica el que instala el hook.)

// setRiel(camara, riel [, offsetRiel]) -> true si quedo puesto.
// Cambia el riel por el que viaja la camara. 'riel' es el objeto Curve o su nombre;
// nil SUELTA el riel (la camara se queda donde esta). El tercer argumento, opcional,
// fija de paso el offsetRiel EN NODOS (cada riel puede tener el suyo), asi el cambio
// es una sola llamada atomica. La camara se reposiciona YA sobre el riel nuevo -no
// queda un frame en el viejo- y el seguimiento arranca de cero.
static int LSetRiel(lua_State* L) {
    Object* cam = W3dScriptParamObjeto(L, 1);
    if (!cam || !W3dCamaraRielSetHook) { lua_pushboolean(L, 0); return 1; }
    const bool soltar = lua_isnoneornil(L, 2);
    const char* nombre = NULL;
    Object* curva = NULL;
    if (!soltar) {
        if (lua_isstring(L, 2) && !lua_isnumber(L, 2)) nombre = lua_tostring(L, 2);
        else                                           curva  = W3dScriptParamObjeto(L, 2);
    }
    int off = 0; const int* poff = NULL;
    if (lua_isnumber(L, 3)) { off = (int)lua_tonumber(L, 3); poff = &off; }
    lua_pushboolean(L, W3dCamaraRielSetHook(cam, curva, nombre, soltar, poff) ? 1 : 0);
    return 1;
}

// setRielNodo(camara, indice) -> true.
// indice >= 0: la camara se PLANTA en ese nodo del riel actual (float: interpola entre
// nodo y nodo) y deja de proyectarse sobre el target. Es lo que pide un tramo donde el
// indice de camara ES el progreso de un recorrido y el ease viene horneado en el
// ESPACIADO de los nodos: muestrear por indice lo respeta, reparametrizar por arco lo
// borraria. indice < 0 (o -1): vuelve al SEGUIMIENTO normal.
static int LSetRielNodo(lua_State* L) {
    Object* cam = W3dScriptParamObjeto(L, 1);
    if (!cam || !W3dCamaraRielNodoHook) { lua_pushboolean(L, 0); return 1; }
    float n = (float)luaL_optnumber(L, 2, -1.0);
    lua_pushboolean(L, W3dCamaraRielNodoHook(cam, n) ? 1 : 0);
    return 1;
}

// setMiradaRiel(camara [, on]) -> true.
// on = true (default del argumento): la camara deja de MIRAR al target y toma el
// yaw/pitch/roll AUTORAL grabado en el nodo del riel (interpolado por arco corto). El
// target sigue decidiendo DONDE se para, no hacia donde mira. on = false: look-at de
// siempre, que es el default de todo proyecto.
// Solo tiene efecto si el .cap trae el canal 'r'; sin el, se sigue mirando al target.
static int LSetMiradaRiel(lua_State* L) {
    Object* cam = W3dScriptParamObjeto(L, 1);
    if (!cam || !W3dCamaraRielMiradaHook) { lua_pushboolean(L, 0); return 1; }
    const bool on = lua_isnoneornil(L, 2) ? true : (lua_toboolean(L, 2) != 0);
    lua_pushboolean(L, W3dCamaraRielMiradaHook(cam, on) ? 1 : 0);
    return 1;
}

// rielDe(camara) -> nombre, offsetRiel, nodo, miradaRiel, indice. Sin riel devuelve
// "", 0, -1, false, -1. Es el getter que usan los asserts del harness y las pruebas
// .w3s. El 5to valor es el INDICE FRACCIONARIO donde la camara quedo parada este
// tick (seguimiento o nodo manual, offset ya restado): es el `t` que una VisZona
// modo curva espera en setVisCurvaT(zona, t) para que la playlist de celdas del
// `.w3dvis` avance nodo a nodo con la camara.
static int LRielDe(lua_State* L) {
    Object* cam = W3dScriptParamObjeto(L, 1);
    const char* nombre = ""; int off = 0; float nodo = -1.0f; bool mirada = false;
    float indice = -1.0f;
    if (cam && W3dCamaraRielEstadoHook)
        W3dCamaraRielEstadoHook(cam, &nombre, &off, &nodo, &mirada, &indice);
    lua_pushstring(L, nombre ? nombre : "");
    lua_pushnumber(L, off);
    lua_pushnumber(L, nodo);
    lua_pushboolean(L, mirada ? 1 : 0);
    lua_pushnumber(L, indice);
    return 5;
}

// ---------------------------------------------------------------------------
//  EL LENTE DE LA CAMARA desde lua: fov + planos de recorte.
//
//    lenteDe(camara) -> fov, cerca, lejos      (grados, unidades de mundo)
//    setLente(camara [, fov] [, cerca] [, lejos]) -> true
//
//  Los tres son propiedades de la camara que ya viven en el .w3d; lo que faltaba
//  era poder moverlas EN JUEGO. El caso tipico del plano lejano es cambiar el
//  alcance del dibujado al entrar a un espacio cerrado, para que no asome
//  geometria lejana que ahi no corresponde -- y de paso ahorrarsela.
//  Cada argumento es opcional: nil = no tocar ese. Se validan igual que el .w3d
//  (fov entre 1 y 179; cerca > 0; lejos > cerca), asi un valor absurdo desde un
//  script no deja la camara sin nada que dibujar.
// ---------------------------------------------------------------------------
static int LLenteDe(lua_State* L) {
    Object* cam = W3dScriptParamObjeto(L, 1);
    float fov = 0, cerca = 0, lejos = 0;
    if (!cam || !W3dCamaraLenteHook || !W3dCamaraLenteHook(cam, &fov, &cerca, &lejos)) {
        lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
        return 3;
    }
    lua_pushnumber(L, fov); lua_pushnumber(L, cerca); lua_pushnumber(L, lejos);
    return 3;
}
static int LSetLente(lua_State* L) {
    Object* cam = W3dScriptParamObjeto(L, 1);
    if (!cam || !W3dCamaraSetLenteHook) { lua_pushboolean(L, 0); return 1; }
    float f = 0, c = 0, l = 0;
    const float* pf = lua_isnumber(L, 2) ? (f = (float)lua_tonumber(L, 2), &f) : NULL;
    const float* pc = lua_isnumber(L, 3) ? (c = (float)lua_tonumber(L, 3), &c) : NULL;
    const float* pl = lua_isnumber(L, 4) ? (l = (float)lua_tonumber(L, 4), &l) : NULL;
    lua_pushboolean(L, W3dCamaraSetLenteHook(cam, pf, pc, pl) ? 1 : 0);
    return 1;
}

// ---------------------------------------------------------------------------
//  ADAPTADOR 2D de la FISICA del Core (physics/W3dFisica). El Core no conoce el
//  lienzo del juego ni Elemento2D (viven aca, en main/), asi que le prestamos
//  estas dos funciones: con ellas su fisica trabaja en px de LIENZO, el MISMO
//  espacio que posPx/tamPx/chocan, y las cajas de rebote son los mismos rects
//  que compara chocan(). Se registra en BindsJuegoRegistrar -> lo tienen igual
//  el Play del editor y el juego compilado.
// ---------------------------------------------------------------------------
static void FisicaLienzo(float* w, float* h) { TamLienzo(w, h); }
static bool FisicaTam2D(Object* o, float* ancho, float* alto) {
    *ancho = 0.0f; *alto = 0.0f;
    if (!o) return false;
    float w, h; TamLienzo(&w, &h);
    if (o->getType() == ObjectType::ui) {   // la raiz de la escena "mide" todo el lienzo
        *ancho = w; *alto = h; return true;
    }
    if (!UI2D_EsElemento2D(o)) return false;
    TamDeclarado((Elemento2D*)o, w, h, ancho, alto);
    return true;
}
// rect RESUELTO por el layout (lo mismo que cajaUI): la fisica lo usa para los objetos que hacen
// de AREA (rebotarEn(obj, cancha)) y para los OBSTACULOS no autoritativos de rebotar(), que estan
// anclados/acomodados por el .w3dui y por eso no estan donde dice su pos cruda. false = no se
// dibujo aun -> el Core cae a su caja normal.
static bool FisicaCajaUI(Object* o, float* cx, float* cy, float* ancho, float* alto) {
    return RectResueltoLienzo(o, cx, cy, ancho, alto);
}
// la pos cruda del objeto es autoritativa? La MISMA regla crudo-vs-resuelto de chocan
// (PosCrudaAutoritativa): la fisica la usa para elegir la caja del OBSTACULO de
// rebotar(obj, otro) -> una pala movida por el script ESTE frame colisiona en su pos
// nueva (sin lag), y un obstaculo puesto por el layout colisiona donde se dibuja.
static bool FisicaPosCruda(Object* o) {
    return PosCrudaAutoritativa(o);
}

// ============================================================================
//  TRES BINDS QUE ESTABAN SOLO EN EL EDITOR — Y ESE ERA EL BUG.
//
//  camaraXZ(), objetivo() y parametro() se registraban en ScriptUI.cpp, que el
//  juego compilado NO linkea. O sea: el MISMO .lua tenia tres funciones menos en
//  el .deb que en el Play. En el demo eso se veia como "el juego compilado no
//  tiene nada que ver con el editor": el controlador del personaje llama a
//  camaraXZ() para mover a Crash relativo a la camara, en el binario era `nil`,
//  y actualizar() reventaba con "attempt to call a nil value" TODOS LOS FRAMES
//  -- o sea que la mitad de abajo del script (sonidos, HUD, gameplay) no corria
//  nunca. Ahora los tres viven aca, en el set COMPARTIDO, que es lo que dice el
//  encabezado de este archivo: un juego corre IGUAL en Play y compilado.
//
//  Los dos que necesitan objetos del editor (la camara activa, el target del
//  objeto Script) entran por HOOK, igual que pantallaDe/setRiel/rielDe: los
//  instala quien linkee esos objetos, y donde no haya camara el bind devuelve un
//  default sensato en vez de nil (un juego que lo llama sin guarda NO se cae).
// ============================================================================
// camaraXZ() -> fx, fz, rx, rz (adelante y derecha de la camara, en el plano XZ)
static int LCamaraXZ(lua_State* L) {
    float fx = 0.0f, fz = -1.0f, rx = 1.0f, rz = 0.0f;   // default: sin camara
    if (W3dCamaraXZHook) W3dCamaraXZHook(&fx, &fz, &rx, &rz);
    lua_pushnumber(L, fx); lua_pushnumber(L, fz);
    lua_pushnumber(L, rx); lua_pushnumber(L, rz);
    return 4;
}
// objetivo() -> el target del objeto dueno del script; nil si no tiene
static int LObjetivo(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_duenio");
    Object* duenio = (Object*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    Object* t = (duenio && W3dObjetivoHook) ? W3dObjetivoHook(duenio) : 0;
    if (t) lua_pushlightuserdata(L, t);
    else   lua_pushnil(L);
    return 1;
}
// parametro("piso" [, default]) -> alias NUMERICO de propiedad() (ver el
// comentario largo que tenia en ScriptUI.cpp). Se implementa LLAMANDO a
// propiedad() para que las dos no puedan divergir.
static int LParametro(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    const lua_Number def = luaL_optnumber(L, 2, 0.0);
    lua_getglobal(L, "propiedad");
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
// controles() -> cuantos mandos hay enchufados AHORA (0 = ninguno: el script
// puede pausar solo). control(n) -> nombre, tipo del n-esimo (1-based, como todo
// en lua); sin ese control devuelve nil. El registro vive mas abajo y lo llenan
// los eventos de hotplug, asi que esto vale en el editor y en el compilado.
static int LControles(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)W3dControlesCuantos());
    return 1;
}
static int LControl(lua_State* L) {
    int n = (int)luaL_optinteger(L, 1, 1) - 1;
    if (n < 0 || n >= W3dControlesCuantos()) { lua_pushnil(L); return 1; }
    lua_pushstring(L, W3dControlNombre(n));
    lua_pushstring(L, W3dControlTipo(n));
    return 2;
}

// ---------------------------------------------------------------------------
//  REGISTRO + ALIMENTACION.
// ---------------------------------------------------------------------------
void BindsJuegoRegistrar(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    W3dFisicaSetAdaptador2D(FisicaLienzo, FisicaTam2D, FisicaCajaUI,
                            FisicaPosCruda);   // idempotente (se llama por cada lua_State)
    lua_pushcfunction(L, LPantalla);    lua_setglobal(L, "pantalla");
    lua_pushcfunction(L, LPosPx);       lua_setglobal(L, "posPx");
    lua_pushcfunction(L, LSetPosPx);    lua_setglobal(L, "setPosPx");
    lua_pushcfunction(L, LTamPx);       lua_setglobal(L, "tamPx");
    lua_pushcfunction(L, LSetTamPx);    lua_setglobal(L, "setTamPx");
    lua_pushcfunction(L, LSetTexto);    lua_setglobal(L, "setTexto");
    lua_pushcfunction(L, LSetTextura);  lua_setglobal(L, "setTextura");
    lua_pushcfunction(L, LSonido);      lua_setglobal(L, "sonido");
    lua_pushcfunction(L, LPararSonido); lua_setglobal(L, "pararSonido");
    lua_pushcfunction(L, LSetSector);   lua_setglobal(L, "setSector");
    lua_pushcfunction(L, LVisCelda);      lua_setglobal(L, "visCelda");
    lua_pushcfunction(L, LSetVisAncla);   lua_setglobal(L, "setVisAncla");
    lua_pushcfunction(L, LSetVisCurvaT);  lua_setglobal(L, "setVisCurvaT");
    lua_pushcfunction(L, LSetLote);       lua_setglobal(L, "setLote");
    lua_pushcfunction(L, LEmitir);      lua_setglobal(L, "emitir");
    lua_pushcfunction(L, LMostrar);     lua_setglobal(L, "mostrar");
    lua_pushcfunction(L, LSetOpacidad); lua_setglobal(L, "setOpacidad");
    lua_pushcfunction(L, LSetTam);      lua_setglobal(L, "setTam");
    lua_pushcfunction(L, LSalir);       lua_setglobal(L, "salir");
    // facilidades: logica generica del juego (una linea en lua)
    lua_pushcfunction(L, LApretado);    lua_setglobal(L, "apretado");
    lua_pushcfunction(L, LChocan);      lua_setglobal(L, "chocan");
    lua_pushcfunction(L, LLimitar);     lua_setglobal(L, "limitar");
    lua_pushcfunction(L, LDentro);      lua_setglobal(L, "dentro");
    lua_pushcfunction(L, LEscala);      lua_setglobal(L, "escala");
    lua_pushcfunction(L, LAreaSegura);  lua_setglobal(L, "areaSegura");
    lua_pushcfunction(L, LFundir);      lua_setglobal(L, "fundir");
    lua_pushcfunction(L, LCajaUI);      lua_setglobal(L, "cajaUI");
    lua_pushcfunction(L, LPantallaDe);  lua_setglobal(L, "pantallaDe");
    // el RIEL de la camara, cambiable en juego (area abierta / elevador / sala lateral)
    lua_pushcfunction(L, LSetRiel);     lua_setglobal(L, "setRiel");
    lua_pushcfunction(L, LSetRielNodo); lua_setglobal(L, "setRielNodo");
    lua_pushcfunction(L, LSetMiradaRiel); lua_setglobal(L, "setMiradaRiel");
    lua_pushcfunction(L, LRielDe);      lua_setglobal(L, "rielDe");
    lua_pushcfunction(L, LLenteDe);     lua_setglobal(L, "lenteDe");
    lua_pushcfunction(L, LSetLente);    lua_setglobal(L, "setLente");
    // los mandos enchufados (hotplug): cuantos hay y como se llama cada uno
    lua_pushcfunction(L, LControles);   lua_setglobal(L, "controles");
    lua_pushcfunction(L, LControl);     lua_setglobal(L, "control");
    // los tres que estaban SOLO en el editor (ver el bloque de arriba)
    lua_pushcfunction(L, LCamaraXZ);    lua_setglobal(L, "camaraXZ");
    lua_pushcfunction(L, LObjetivo);    lua_setglobal(L, "objetivo");
    lua_pushcfunction(L, LParametro);   lua_setglobal(L, "parametro");
    // multi-escena: cambiarEscena(nombre[,reiniciar]) — mismo bind en editor y runtime
    W3dEscenaRegistrarBind(Lv);
}

void BindsJuegoRender(float esc, float x0, float y0,
                      float safeWinX, float safeWinY, float safeWinW, float safeWinH,
                      const std::vector<UI2DPos>* posiciones) {
    gUltEsc = esc; gUltX0 = x0; gUltY0 = y0;
    gSafeWinX = safeWinX; gSafeWinY = safeWinY; gSafeWinW = safeWinW; gSafeWinH = safeWinH;
    gUltPos.clear();
    if (posiciones) gUltPos = *posiciones;
}

void BindsJuegoPunto(int slot, float cx, float cy, bool activo) {
    if (slot < 0 || slot >= W3D_GAME_MAXDEDOS) return;
    gPunX[slot] = cx; gPunY[slot] = cy; gPunAct[slot] = activo;
}

void BindsJuegoRaton(float cx, float cy, bool presente) {
    gRatonCX = cx; gRatonCY = cy; gRatonPresente = presente;
}

bool BindsJuegoRatonEstado(float* cx, float* cy) {
    if (cx) *cx = gRatonCX;
    if (cy) *cy = gRatonCY;
    return gRatonPresente;
}

void BindsJuegoSnapshotPunteros(void) {
    // el flanco de apretado() del PROXIMO frame compara contra ESTE estado; se hace DESPUES de correr
    // los scripts para que todas las llamadas de ESTE frame vean el mismo "estado anterior".
    for (int i = 0; i < W3D_GAME_MAXDEDOS; i++) gPunPrev[i] = gPunAct[i];
}

void BindsJuegoResetPunteros(void) {
    for (int i = 0; i < W3D_GAME_MAXDEDOS; i++) {
        gPunAct[i] = false; gPunPrev[i] = false; gPunX[i] = 0.0f; gPunY[i] = 0.0f;
    }
    // al parar/reiniciar tambien se olvida el mouse: sin hover colgado de una corrida vieja
    gRatonPresente = false; gRatonCX = 0.0f; gRatonCY = 0.0f;
}

// ---------------------------------------------------------------------------
//  LOS CONTROLES CONECTADOS (ver BindsJuego.h): el registro compartido.
//  Aca NO entra SDL: quien maneja los eventos (el editor en main.cpp, el juego
//  compilado en su main generado) abre/cierra el device y llama a estas dos; el
//  aviso (toast + consola) sale igual en los dos lados porque se emite ACA, una
//  sola vez, en vez de estar copiado en cada plataforma.
// ---------------------------------------------------------------------------
struct W3dControlReg {
    void*       handle;
    int         id;
    std::string nombre;
    std::string tipo;
};
static std::vector<W3dControlReg> gControles;

int W3dControlesCuantos(void) { return (int)gControles.size(); }
const char* W3dControlNombre(int i) {
    return (i >= 0 && i < (int)gControles.size()) ? gControles[(size_t)i].nombre.c_str() : "";
}
const char* W3dControlTipo(int i) {
    return (i >= 0 && i < (int)gControles.size()) ? gControles[(size_t)i].tipo.c_str() : "";
}
void* W3dControlHandle(int i) {
    return (i >= 0 && i < (int)gControles.size()) ? gControles[(size_t)i].handle : 0;
}
int W3dControlIdEn(int i) {
    return (i >= 0 && i < (int)gControles.size()) ? gControles[(size_t)i].id : -1;
}

bool W3dControlAlta(void* handle, int id, const char* nombre, const char* tipo) {
    for (size_t i = 0; i < gControles.size(); i++)
        if (gControles[i].id == id) return false;      // ya estaba (evento repetido)
    W3dControlReg r;
    r.handle = handle;
    r.id     = id;
    r.nombre = (nombre && *nombre) ? nombre : "Control";
    r.tipo   = (tipo && *tipo) ? tipo : "generico";
    gControles.push_back(r);
    printf("[control] se detecto un nuevo control: %s (%s), %d conectado(s)\n",
           r.nombre.c_str(), r.tipo.c_str(), (int)gControles.size());
    fflush(stdout);
    W3dAvisof(false, "Se detecto un nuevo control: %s", W3dNombreCorto(r.nombre).c_str());
    return true;
}

bool W3dControlBaja(int id, void** handle) {
    if (handle) *handle = 0;
    for (size_t i = 0; i < gControles.size(); i++) {
        if (gControles[i].id != id) continue;
        std::string nom = gControles[i].nombre;
        if (handle) *handle = gControles[i].handle;
        gControles.erase(gControles.begin() + (long)i);
        printf("[control] se desconecto un control: %s, quedan %d\n",
               nom.c_str(), (int)gControles.size());
        fflush(stdout);
        W3dAvisof(true, "Se desconecto un control: %s", W3dNombreCorto(nom).c_str());
        return true;
    }
    return false;
}

bool BindsJuegoQuierePararse(void) { return gQuit; }
// mismo pedido, pero desde el RUNTIME (no desde lua): lo usa la captura de
// verificacion del juego compilado, que guarda un frame a PNG y cierra.
void BindsJuegoPedirSalir(void) { gQuit = true; }
