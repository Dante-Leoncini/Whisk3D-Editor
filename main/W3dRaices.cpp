// ============================================================================
//  W3dRaices.cpp — ver W3dRaices.h. Las escenas 3D, los juegos y los prefabs de un
//  proyecto como RAICES: el registro, el tipo de cada una, el CONTEXTO de cada raiz
//  (lo que se guarda mientras no es la activa), el cambio de raiz, la carga perezosa
//  y el cambiarEscena() de escenas/juegos 3D. Compila en el editor y en el runtime 3D.
//  C++03.
// ============================================================================
#include "W3dRaices.h"
#include "W3dEscena.h"                   // cambiarEscena(): el pedido de una escena 3D llega por aca
#include "objects/Objects.h"
#include "objects/Scene.h"
#include "objects/Empty.h"               // el objeto raiz de un prefab nuevo
#include "objects/Light.h"               // las luces son POR RAIZ (el tope de 8 de GL)
#include "objects/Camera.h"              // CameraActive: una por raiz
#include "objects/Armature.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"         // W3dMallaCarpetaNormalizar: la misma regla de carpetas que los demas recursos
#include "animation/Animation.h"         // animaciones de escena: una lista por raiz
#include "animation/W3dAnimSet.h"        // las vistas de los clips de jerarquia (su clip, su version)
#include "animation/SkeletalAnimation.h" // g_mixEscenas / g_animMix: el Mix de escenas (por raiz)
#include "script/W3dScript.h"            // W3dScriptDatos: una raiz con scripts es un JUEGO (proyectos viejos)
#include "render/OpcionesRender.h"       // g_redraw
#include "io/JsonW3d.h"
#include "base/W3dNombres.h"
#include "w3dGraphics.h"
#include "w3dlog.h"

// ============================================================================
//  EL CONTEXTO DE UNA RAIZ: lo que es "de la escena que se esta mirando" y vive en
//  globales del editor/Core. Mientras la raiz no es la activa, se guarda aca; al
//  volver se restaura tal cual (seleccion, camara, luces, animaciones).
// ============================================================================
struct W3dContextoRaiz {
    std::vector<Object*> selects;
    Object* activo;
    Object* coleccion;
    Camera* camara;
    std::vector<SceneAnimation*> escenas;   // SceneAnimations
    std::vector<AnimationObject> curvas;    // AnimationObjects (las de la animacion de escena activa)
    int escenaActiva;                       // SceneAnimActiva
    int animKind;                           // ActiveAnimKind: el MODO del timeline de esta raiz (Juego, un
                                            // clip, su animacion de escena...); 1/3/4 apuntan a objetos suyos
    Armature* animArm;
    Mesh* animMesh;
    bool esJuego;                           // AnimEsJuego (el "Juego" de un JUEGO: timeline sin keyframes)
    bool modoMix;                           // g_animMix (el modo Mix es de la raiz que lo tiene armado)
    std::vector<W3dCapaAnim> mix;           // g_mixEscenas
    int mixActiva;                          // g_mixEscenaActiva
    std::vector<Light*> luces;              // Lights
    int inicio, fin, fps;                   // StartFrame / EndFrame / AnimFPS
    int frame;                              // CurrentFrame: cada raiz con su cabezal (un juego, en su frame 1)
    W3dContextoRaiz() : activo(0), coleccion(0), camara(0), escenaActiva(0), animKind(0), animArm(0),
                        animMesh(0), esJuego(false), modoMix(false), mixActiva(-1), inicio(1), fin(250), fps(30),
                        frame(1) {}
};

static bool KindDeObjeto(int k) { return k == 1 || k == 3 || k == 4; }

// mueve las globales al contexto y las deja VACIAS (una raiz sin nada elegido)
static void GuardarContexto(W3dContextoRaiz* c) {
    // la vista de un clip de jerarquia que se estaba editando vuelve a su clip (otra raiz lo puede usar)
    W3dClipsVistasSincronizar();
    c->selects.swap(ObjSelects); ObjSelects.clear();
    c->activo = ObjActivo; ObjActivo = NULL;
    c->coleccion = CollectionActive; CollectionActive = NULL;
    c->camara = CameraActive; CameraActive = NULL;
    c->escenas.swap(SceneAnimations); SceneAnimations.clear();
    c->curvas.swap(AnimationObjects); AnimationObjects.clear();
    c->escenaActiva = SceneAnimActiva; SceneAnimActiva = 0;
    c->animKind = ActiveAnimKind; c->animArm = ActiveAnimArm; c->animMesh = ActiveAnimMesh;
    c->esJuego = AnimEsJuego; c->modoMix = g_animMix;
    // el modo del timeline es de ESTA raiz (la que entra pone el suyo: RestaurarContexto / W3dRaizUsar)
    ActiveAnimKind = 0; AnimEsJuego = false; g_animMix = false;
    ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    c->mix.swap(g_mixEscenas); g_mixEscenas.clear();
    c->mixActiva = g_mixEscenaActiva; g_mixEscenaActiva = -1;
    c->luces.swap(Lights); Lights.clear();
    c->inicio = StartFrame; c->fin = EndFrame; c->fps = AnimFPS;
    c->frame = CurrentFrame;
}
// al reves: el contexto vuelve a las globales (las que habia se pisan: GuardarContexto las vacio)
static void RestaurarContexto(W3dContextoRaiz* c) {
    ObjSelects.swap(c->selects);
    ObjActivo = c->activo;
    CollectionActive = c->coleccion ? c->coleccion : SceneCollection;
    CameraActive = c->camara;
    SceneAnimations.swap(c->escenas);
    AnimationObjects.swap(c->curvas);
    SceneAnimActiva = c->escenaActiva;
    // el modo del timeline tal cual se dejo (un clip cuyo objeto se borro mientras tanto vuelve a la
    // animacion de escena: DesvincularContextos le solto el puntero)
    ActiveAnimKind = c->animKind; ActiveAnimArm = c->animArm; ActiveAnimMesh = c->animMesh;
    if (KindDeObjeto(ActiveAnimKind) && !ActiveAnimArm && !ActiveAnimMesh) ActiveAnimKind = 0;
    AnimEsJuego = c->esJuego; g_animMix = c->modoMix;
    g_mixEscenas.swap(c->mix);
    g_mixEscenaActiva = c->mixActiva;
    Lights.swap(c->luces);
    StartFrame = c->inicio; EndFrame = c->fin; AnimFPS = c->fps;
    CurrentFrame = c->frame;
    InitSceneAnimations();   // (una raiz recien creada no tiene ninguna: nace la "Scene")
    // la vista de un clip que se dejo elegida: si el clip cambio mientras tanto (se edito desde otra raiz,
    // es compartido), se re-ata
    if (W3dClipVistaViva(SceneAnimActiva) &&
        SceneAnimations[(size_t)SceneAnimActiva]->versionVista != SceneAnimations[(size_t)SceneAnimActiva]->clipJer->version)
        W3dClipVistaLeer(SceneAnimActiva);
}
// un contexto que se DESCARTA (la raiz se libera): sus animaciones de escena son suyas
static void BorrarContexto(W3dContextoRaiz* c) {
    if (!c) return;
    for (size_t i = 0; i < c->escenas.size(); i++) delete c->escenas[i];
    delete c;
}

// ============================================================================
//  EL REGISTRO
// ============================================================================
static std::vector<W3dRaizFila> gFilas;
static int gActiva = -1;
static std::string gInicial;
// una fila entro (delta +1) o salio (-1) en la posicion 'desde': los indices de fila que se guardan (la
// activa, el cambiarEscena pedido, la cinematica en curso) la siguen. Esta abajo, con el juego.
static void CorrerIndices(int desde, int delta);

// la raiz actual es una Scene? (el editor y el runtime 3D arrancan con una; el runtime 2D no)
static Scene* SceneDe(Object* r) { return (r && scene && (Object*)scene == r) ? scene : NULL; }

static void Asegurar() {
    if (!gFilas.empty() || !SceneCollection) return;
    W3dRaizFila f;
    f.tipo = W3D_RAIZ_ESCENA;   // (sin tipoExplicito: escena o juego segun tenga scripts, ver W3dRaizTipoDe)
    f.nombre = W3dRaizNombrePorDefecto(W3D_RAIZ_ESCENA);
    f.raiz = SceneCollection;
    f.escena = SceneDe(SceneCollection);
    f.bloque = true;
    gFilas.push_back(f);
    gActiva = 0;
    W3dRaizRegistrar(SceneCollection);
}

const std::vector<W3dRaizFila>& W3dRaices() { Asegurar(); return gFilas; }
int W3dRaizActiva() { Asegurar(); return gActiva; }
Object* W3dRaizCamaraActiva(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size() || !gFilas[(size_t)idx].raiz) return NULL;
    if (idx == gActiva) return (Object*)CameraActive;
    const W3dContextoRaiz* c = gFilas[(size_t)idx].ctx;
    return c ? (Object*)c->camara : NULL;
}
const char* W3dRaizNombrePorDefecto(int tipo) {
    return tipo == W3D_RAIZ_PREFAB ? "Prefab" : (tipo == W3D_RAIZ_JUEGO ? "Game" : "Scene");
}
const char* W3dRaizTipoClave(int tipo) {
    return tipo == W3D_RAIZ_PREFAB ? "prefab" : (tipo == W3D_RAIZ_JUEGO ? "juego" : "escena");
}
int W3dRaizTipoDeClave(const std::string& c, int defecto) {
    if (c == "escena") return W3D_RAIZ_ESCENA;
    if (c == "juego")  return W3D_RAIZ_JUEGO;
    if (c == "prefab") return W3D_RAIZ_PREFAB;
    return defecto;
}

int W3dRaizBuscar(int tipo, const std::string& nombre) {
    Asegurar();
    const int clase = W3dRaizClase(tipo);
    for (size_t i = 0; i < gFilas.size(); i++)
        if (W3dRaizClase(gFilas[i].tipo) == clase && gFilas[i].nombre == nombre) return (int)i;
    return -1;
}

// ---- EL TIPO ----
static bool TieneScripts(Object* o) {
    if (!o) return false;
    if (o->scriptDatos && !o->scriptDatos->scripts.empty()) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (TieneScripts(o->Childrens[i])) return true;
    return false;
}
int W3dRaizTipoDeducido(Object* raiz) { return TieneScripts(raiz) ? W3D_RAIZ_JUEGO : W3D_RAIZ_ESCENA; }
int W3dRaizTipoDe(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return -1;
    W3dRaizFila& f = gFilas[(size_t)idx];
    // sin tipo explicito y cargada: el de su contenido (y la fila lo recuerda para quien la mire)
    if (!f.tipoExplicito && W3dRaizEs3D(f.tipo) && f.raiz) f.tipo = W3dRaizTipoDeducido(f.raiz);
    return f.tipo;
}
int W3dRaizTipoActiva() {
    Asegurar();
    return gActiva >= 0 ? W3dRaizTipoDe(gActiva) : W3D_RAIZ_ESCENA;
}

void W3dRaizModoPorTipo(bool forzar) {
#ifndef W3D_SIN_EDITOR
    // (el runtime compilado no tiene timeline: siempre esta jugando)
    const int t = W3dRaizTipoActiva();
    if (t == W3D_RAIZ_JUEGO) {
        if (!forzar && ActiveAnimKind != 2) return;   // el juego estaba editando uno de sus clips: se respeta
        ActiveAnimKind = 2; AnimEsJuego = true; g_animMix = false;
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
        StartFrame = 1;
        // sin partida el juego esta en su estado base: su frame 1 (con partida, el cabezal es el tick)
        extern bool SimActiva();
        if (!SimActiva() || CurrentFrame < StartFrame) CurrentFrame = StartFrame;
    } else if (ActiveAnimKind == 2 || AnimEsJuego) {
        // una escena (o un prefab) no simula: su timeline es su animacion de escena
        ActiveAnimKind = 0; AnimEsJuego = false;
        InitSceneAnimations();
        AnimCargarRangoActivo();
        if (CurrentFrame < StartFrame || CurrentFrame > EndFrame) CurrentFrame = StartFrame;
    }
#else
    (void)forzar;
#endif
}

bool W3dRaizFijarTipo(int idx, int tipo) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return false;
    W3dRaizFila& f = gFilas[(size_t)idx];
    if (!W3dRaizEs3D(f.tipo) || !W3dRaizEs3D(tipo)) return false;
    f.tipo = tipo;
    f.tipoExplicito = true;
    if (idx == gActiva) W3dRaizModoPorTipo(true);
    return true;
}
int W3dRaizDeObjeto(Object* raiz) {
    if (!raiz) return -1;
    Asegurar();
    for (size_t i = 0; i < gFilas.size(); i++) if (gFilas[i].raiz == raiz) return (int)i;
    return -1;
}
void W3dRaicesEnOrden(std::vector<Object*>& out) {
    out.clear();
    Asegurar();
    for (size_t i = 0; i < gFilas.size(); i++) if (gFilas[i].raiz) out.push_back(gFilas[i].raiz);
    if (out.empty() && SceneCollection) out.push_back(SceneCollection);
}
int W3dRaizBloque() {
    Asegurar();
    for (size_t i = 0; i < gFilas.size(); i++) if (gFilas[i].bloque) return (int)i;
    // se borro la del bloque: la primera escena O JUEGO pasa a serlo (su entrada se descarta al guardar). Un
    // juego tambien: el lector acepta cualquiera de los dos como la del bloque (la fila de "escenas3d" sin
    // entrada), y sin ninguna el guardado escribia la raiz activa en el bloque "escena" Y en su entrada (al
    // reabrir aparecia una escena "Scene" fantasma con sus objetos repetidos, y el juego arrancaba en ella)
    for (size_t i = 0; i < gFilas.size(); i++)
        if (W3dRaizEs3D(gFilas[i].tipo)) {
            gFilas[i].bloque = true;
            if (gInicial == gFilas[i].nombre) gInicial.clear();   // (la del bloque ES la inicial por defecto)
            return (int)i;
        }
    return -1;
}

std::string W3dRaizNombreLibre(int tipo, const std::string& base, int excepto) {
    std::vector<std::string> tomados;
    const int clase = W3dRaizClase(tipo);
    for (size_t i = 0; i < gFilas.size(); i++)
        if (W3dRaizClase(gFilas[i].tipo) == clase && (int)i != excepto) tomados.push_back(gFilas[i].nombre);
    const char* def = W3dRaizNombrePorDefecto(tipo);
    return W3dNombreUnicoEnValores(W3dNombreNormalizar(base, def), def, tomados, -1);
}

bool W3dRaicesHayRegistro() {
    if (gFilas.empty()) return false;
    if (gFilas.size() > 1) return true;
    const W3dRaizFila& f = gFilas[0];
    if (!W3dRaizEs3D(f.tipo) || f.nombre != W3dRaizNombrePorDefecto(W3D_RAIZ_ESCENA) ||
        !f.carpeta.empty() || !gInicial.empty()) return true;
    // su tipo solo hace falta escribirlo si NO es el que se deduciria al abrir
    return f.tipoExplicito && f.raiz && f.tipo != W3dRaizTipoDeducido(f.raiz);
}

const std::string& W3dRaizInicial() { return gInicial; }
bool W3dRaizFijarInicial(const std::string& nombre) {
    if (nombre.empty()) { gInicial.clear(); return true; }
    const int i = W3dRaizBuscar(W3D_RAIZ_ESCENA, nombre);
    if (i < 0) return false;
    // la del bloque ES la inicial por defecto: no hace falta nombrarla
    gInicial = gFilas[(size_t)i].bloque ? std::string() : gFilas[(size_t)i].nombre;
    return true;
}
int W3dRaizInicialIdx() {
    const int i = gInicial.empty() ? -1 : W3dRaizBuscar(W3D_RAIZ_ESCENA, gInicial);
    return i >= 0 ? i : W3dRaizBloque();
}

int W3dRaizObjetos(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size() || !gFilas[(size_t)idx].raiz) return -1;
    std::vector<Object*> pila(1, gFilas[(size_t)idx].raiz);
    int n = -1;   // (la raiz misma no cuenta)
    while (!pila.empty()) {
        Object* o = pila.back(); pila.pop_back(); n++;
        for (size_t i = 0; i < o->Childrens.size(); i++) pila.push_back(o->Childrens[i]);
    }
    return n;
}

void W3dRaicesListasAnim(std::vector<std::vector<SceneAnimation*>*>& out) {
    out.clear();
    out.push_back(&SceneAnimations);
    for (size_t i = 0; i < gFilas.size(); i++)
        if (gFilas[i].ctx && (int)i != gActiva) out.push_back(&gFilas[i].ctx->escenas);
}

// ============================================================================
//  CAMBIO DE RAIZ
// ============================================================================
bool W3dRaizUsar(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return false;
    if (idx == gActiva) return true;
    W3dRaizFila& f = gFilas[(size_t)idx];
    if (!f.raiz) return false;
    W3dContextoRaiz* c = new W3dContextoRaiz();
    GuardarContexto(c);
    if (gActiva >= 0 && gActiva < (int)gFilas.size()) gFilas[(size_t)gActiva].ctx = c;
    else BorrarContexto(c);
    gActiva = idx;
    SceneCollection = f.raiz;
    if (f.escena) scene = f.escena;
    if (f.ctx) {
        RestaurarContexto(f.ctx);
        BorrarContexto(f.ctx);   // (vacio: sus vectores ya volvieron a las globales)
        f.ctx = NULL;
        // (tal cual: el guardado y la carga usan este swap para ir y volver. El cambio "de verdad",
        //  W3dRaizCambiarActiva, ademas acomoda el modo del timeline al tipo)
    } else {
        // una raiz que nunca fue la activa (recien creada o cargada): su coleccion activa es ella
        // misma y el timeline arranca en el modo de su tipo (un juego en "Juego", una escena en su
        // animacion de escena "Scene")
        CollectionActive = SceneCollection;
        InitSceneAnimations();
        AnimCargarRangoActivo();
        CurrentFrame = StartFrame;
        W3dRaizModoPorTipo(true);
    }
    W3dAnimCurvasInvalidar();   // (las curvas de la otra raiz: el editor vuelve a posar aunque el frame sea el mismo)
    return true;
}

// las luces GL que dejo encendidas la raiz anterior: la nueva enciende las suyas al dibujar
static void ApagarLucesGL() {
    for (int i = 0; i < MAX_LIGHTS; i++) w3dEngine::SetLightEnabled(GL_LIGHT0 + (unsigned int)i, false);
}

bool W3dRaizCambiarActiva(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return false;
    if (idx == gActiva) return true;
    if (!gFilas[(size_t)idx].raiz && !W3dRaizCargar(idx)) return false;
    if (!W3dRaizUsar(idx)) return false;
    // una escena que quedo en "Juego" (la visito el Play de otra raiz) vuelve a su animacion; un juego
    // que se dejo editando un clip lo sigue editando
    W3dRaizModoPorTipo(false);
    ApagarLucesGL();
    g_redraw = true;
    return true;
}

// ============================================================================
//  CARGA PEREZOSA
// ============================================================================
bool W3dRaizCargar(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return false;
    if (gFilas[(size_t)idx].raiz) return true;
    Scene* r = W3dSceneNuevaRaiz();
    W3dRaizRegistrar(r);
    gFilas[(size_t)idx].raiz = r;
    gFilas[(size_t)idx].escena = r;
    const int antes = gActiva;
    const unsigned int selSerial = W3dSeleccionSerial;   // (cargar no es ELEGIR: ver W3dSeleccionSerial)
    W3dRaizUsar(idx);                        // la carga arma el arbol sobre SceneCollection
    const bool ok = W3dRaizLeerContenido(gFilas[(size_t)idx]);
    gFilas[(size_t)idx].cargaFallo = !ok;
    DeseleccionarTodo(); ObjActivo = NULL;   // (los constructores fueron seleccionando)
    if (antes >= 0) W3dRaizUsar(antes);
    W3dSeleccionSerial = selSerial;          // (el recurso activo de la biblioteca sigue siendo el activo)
    w3dLogf("[raices] %s '%s' cargada desde '%s'%s", W3dRaizTipoClave(W3dRaizTipoDe(idx)),
            gFilas[(size_t)idx].nombre.c_str(), gFilas[(size_t)idx].entrada.c_str(), ok ? "" : " (NO SE PUDO LEER)");
    return true;   // la raiz existe (vacia si no se pudo leer: cargaFallo frena el guardado encima)
}

int W3dRaicesCargarTodas(std::vector<int>* cargadas) {
    Asegurar();
    if (cargadas) cargadas->clear();
    int n = 0;
    for (size_t i = 0; i < gFilas.size(); i++)
        if (!gFilas[i].raiz) {
            W3dRaizCargar((int)i);
            n++;
            if (cargadas) cargadas->push_back((int)i);
        }
    return n;
}

// ============================================================================
//  ALTA / BAJA / NOMBRE / CARPETA
// ============================================================================
int W3dRaizNueva(int tipo, const std::string& nombre) {
    Asegurar();
    W3dRaizFila f;
    f.tipo = (tipo == W3D_RAIZ_PREFAB || tipo == W3D_RAIZ_JUEGO) ? tipo : W3D_RAIZ_ESCENA;
    f.tipoExplicito = true;   // nacio con "New Scene" / "New Game" / "New Prefab": es lo que el usuario pidio
    f.nombre = W3dRaizNombreLibre(f.tipo, nombre, -1);
    Scene* r = W3dSceneNuevaRaiz();
    W3dRaizRegistrar(r);
    f.raiz = r;
    f.escena = r;
    // EN EL ORDEN DEL PROYECTO (ver W3dRaices.h): una escena o un juego va despues de las escenas y juegos
    // que ya hay y ANTES de los prefabs. Al final de todo, un prefab creado antes que una escena cambiaba de
    // lugar al reabrir y el re-guardado recorria las raices en otro orden (el registro "mallas" salia distinto)
    int idx = (int)gFilas.size();
    if (f.tipo != W3D_RAIZ_PREFAB)
        for (size_t i = 0; i < gFilas.size(); i++) if (gFilas[i].tipo == W3D_RAIZ_PREFAB) { idx = (int)i; break; }
    gFilas.insert(gFilas.begin() + idx, f);
    CorrerIndices(idx, +1);
    if (f.tipo == W3D_RAIZ_PREFAB) {
        // el OBJETO RAIZ del prefab (lo que una instancia va a generar): un vacio con su nombre,
        // creado ADENTRO de la raiz nueva (el constructor se cuelga de SceneCollection)
        const int antes = gActiva;
        W3dRaizUsar(idx);
        const bool nc = W3dNombresCargando;
        W3dNombresCargando = true;           // el nombre tal cual (es el unico objeto de la raiz)
        Empty* e = new Empty(NULL, Vector3(0, 0, 0));
        W3dNombresCargando = nc;
        e->name = gFilas[(size_t)idx].nombre;
        CollectionActive = e;                // lo que se importe va adentro del prefab
        if (antes >= 0) W3dRaizUsar(antes);
    }
    return idx;
}

bool W3dRaizRenombrar(int idx, const std::string& nuevo, std::string* final) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return false;
    W3dRaizFila& f = gFilas[(size_t)idx];
    const std::string n = W3dRaizNombreLibre(f.tipo, nuevo, idx);
    if (final) *final = n;
    if (n == f.nombre) return true;
    if (W3dRaizEs3D(f.tipo) && gInicial == f.nombre) gInicial = n;   // la inicial la sigue
    f.nombre = n;
    return true;
}

bool W3dRaizFijarCarpeta(int idx, const std::string& carpeta) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size()) return false;
    gFilas[(size_t)idx].carpeta = carpeta;
    return true;
}

// libera el arbol de una raiz que NO es la activa. Se olvida del registro ANTES (asi ~Object no
// la recorre) y se destruye con ella como SceneCollection: cada ~Object recorre solo lo que se va.
static void LiberarRaiz(W3dRaizFila& f) {
    BorrarContexto(f.ctx); f.ctx = NULL;
    if (!f.raiz) return;
    Object* r = f.raiz;
    f.raiz = NULL; f.escena = NULL;
    W3dRaizOlvidar(r);
    Object* activa = SceneCollection;
    SceneCollection = r;
    W3dLiberarSubarbol(r);
    SceneCollection = activa;
}

// las reglas de un borrado (W3dRaizBorrar y W3dRaizSacar): no la activa ni la ultima escena/juego
static bool PuedeBorrarse(int idx, std::string* motivo) {
    if (idx < 0 || idx >= (int)gFilas.size()) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (idx == gActiva) { if (motivo) *motivo = "It is open: switch to another scene first"; return false; }
    if (W3dCineActiva() && (idx == W3dCineEscena() || idx == W3dCineJuego())) {
        if (motivo) *motivo = "Stop the game first";
        return false;
    }
    if (W3dRaizEs3D(gFilas[(size_t)idx].tipo)) {
        int escenas = 0;   // (escenas y juegos: el proyecto necesita al menos una raiz 3D)
        for (size_t i = 0; i < gFilas.size(); i++) if (W3dRaizEs3D(gFilas[i].tipo)) escenas++;
        if (escenas <= 1) { if (motivo) *motivo = "A project needs at least one scene"; return false; }
    }
    return true;
}

bool W3dRaizBorrar(int idx, std::string* motivo) {
    Asegurar();
    if (!PuedeBorrarse(idx, motivo)) return false;
    // (sobre la fila del registro, no una copia: ~Object mira los contextos mientras se libera)
    LiberarRaiz(gFilas[(size_t)idx]);
    const W3dRaizFila f = gFilas[(size_t)idx];
    if (W3dRaizEs3D(f.tipo) && gInicial == f.nombre) gInicial.clear();
    gFilas.erase(gFilas.begin() + idx);
    CorrerIndices(idx, -1);
    if (f.bloque) W3dRaizBloque();   // otra escena pasa a ser la del bloque
    w3dLogf("[raices] %s '%s' borrada", W3dRaizTipoClave(f.tipo), f.nombre.c_str());
    return true;
}

bool W3dRaizSacar(int idx, W3dRaizSacada* out, std::string* motivo) {
    Asegurar();
    if (!out || !PuedeBorrarse(idx, motivo)) return false;
    if (!gFilas[(size_t)idx].raiz) W3dRaizCargar(idx);   // (el Ctrl+Z la devuelve de memoria)
    out->fila = gFilas[(size_t)idx];
    out->pos = idx;
    out->inicial = W3dRaizEs3D(out->fila.tipo) && !gInicial.empty() && gInicial == out->fila.nombre;
    if (out->inicial) gInicial.clear();
    gFilas.erase(gFilas.begin() + idx);
    CorrerIndices(idx, -1);
    if (out->fila.bloque) W3dRaizBloque();   // otra escena pasa a ser la del bloque (hasta el Ctrl+Z)
    // (su arbol sigue REGISTRADO: ver W3dRaices.h)
    w3dLogf("[raices] %s '%s' borrada (con undo)", W3dRaizTipoClave(out->fila.tipo), out->fila.nombre.c_str());
    return true;
}

bool W3dRaizDevolver(W3dRaizSacada& s) {
    Asegurar();
    if (s.pos < 0) return false;
    W3dRaizFila f = s.fila;
    // (entre el borrado y el Ctrl+Z nadie puede tomar su nombre sin cambiar de raiz -lo que vacia el
    //  historial-, pero dos filas con el mismo nombre no pueden quedar)
    f.nombre = W3dRaizNombreLibre(f.tipo, f.nombre, -1);
    // vuelve a ser la del bloque: la que se promovio al sacarla deja de serlo
    if (f.bloque) for (size_t i = 0; i < gFilas.size(); i++) gFilas[i].bloque = false;
    // su lugar, acotado al orden del proyecto (escenas y juegos antes que los prefabs)
    int pos = s.pos > (int)gFilas.size() ? (int)gFilas.size() : s.pos;
    if (f.tipo == W3D_RAIZ_PREFAB) { while (pos < (int)gFilas.size() && gFilas[(size_t)pos].tipo != W3D_RAIZ_PREFAB) pos++; }
    else { while (pos > 0 && gFilas[(size_t)(pos - 1)].tipo == W3D_RAIZ_PREFAB) pos--; }
    gFilas.insert(gFilas.begin() + pos, f);
    CorrerIndices(pos, +1);
    if (f.raiz) W3dRaizRegistrar(f.raiz);
    if (s.inicial && !f.bloque) gInicial = f.nombre;
    w3dLogf("[raices] %s '%s' vuelve (Ctrl+Z)", W3dRaizTipoClave(f.tipo), f.nombre.c_str());
    s.fila = W3dRaizFila(); s.pos = -1; s.inicial = false;   // (ya no es de quien la saco)
    return true;
}

void W3dRaizSacadaLiberar(W3dRaizSacada& s) {
    if (s.pos < 0) return;
    LiberarRaiz(s.fila);
    s.fila = W3dRaizFila(); s.pos = -1;
}

// ============================================================================
//  PROYECTO
// ============================================================================
void W3dRaicesLeerJson(JVal* raiz) {
    // (el cierre del proyecto anterior ya libero las raices y vacio el registro)
    gFilas.clear(); gActiva = -1; gInicial.clear();
    std::vector<W3dRaizFila> filas;
    int bloque = -1;
    JVal* je = raiz ? JHijo(raiz, "escenas3d", 5) : NULL;
    if (je)
        for (size_t i = 0; i < je->lista.size(); i++) {
            JVal* e = je->lista[i];
            if (!e || e->tipo != 4) continue;
            W3dRaizFila f;
            // "tipo": "escena" | "juego". Sin tipo (un registro escrito a mano): el de su contenido
            const std::string tc = JS(e, "tipo", "");
            f.tipo = W3dRaizTipoDeClave(tc, W3D_RAIZ_ESCENA);
            if (f.tipo == W3D_RAIZ_PREFAB) f.tipo = W3D_RAIZ_ESCENA;   // (un prefab no va en esta lista)
            f.tipoExplicito = !tc.empty() && W3dRaizTipoDeClave(tc, -1) >= 0;
            f.nombre = JS(e, "nombre", "");
            f.entrada = JS(e, "entrada", "");
            f.carpeta = W3dMallaCarpetaNormalizar(JS(e, "carpeta", ""));
            // la del bloque "escena" es la que NO tiene entrada (la primera, si viene mas de una)
            if (f.entrada.empty()) { if (bloque < 0) { bloque = (int)filas.size(); f.bloque = true; } else continue; }
            filas.push_back(f);
        }
    if (bloque < 0) {
        // proyecto viejo (o un registro sin la del bloque, escrito a mano): el bloque es una escena mas
        W3dRaizFila f;
        f.tipo = W3D_RAIZ_ESCENA; f.bloque = true;
        filas.insert(filas.begin(), f);
        bloque = 0;
    }
    JVal* jp = raiz ? JHijo(raiz, "prefabs", 5) : NULL;
    if (jp)
        for (size_t i = 0; i < jp->lista.size(); i++) {
            JVal* e = jp->lista[i];
            if (!e || e->tipo != 4) continue;
            W3dRaizFila f;
            f.tipo = W3D_RAIZ_PREFAB;
            f.tipoExplicito = true;
            f.nombre = JS(e, "nombre", "");
            f.entrada = JS(e, "entrada", "");
            f.carpeta = W3dMallaCarpetaNormalizar(JS(e, "carpeta", ""));
            if (f.entrada.empty()) continue;   // un prefab sin entrada no tiene de donde salir
            filas.push_back(f);
        }
    // NOMBRES UNICOS por tipo (un archivo escrito a mano puede repetir o dejar vacio)
    for (size_t i = 0; i < filas.size(); i++) {
        gFilas.push_back(filas[i]);
        gFilas.back().nombre = W3dRaizNombreLibre(filas[i].tipo, filas[i].nombre, (int)gFilas.size() - 1);
    }
    gActiva = bloque;
    gFilas[(size_t)bloque].raiz = SceneCollection;
    gFilas[(size_t)bloque].escena = SceneDe(SceneCollection);
    if (SceneCollection) W3dRaizRegistrar(SceneCollection);
    W3dRaizFijarInicial(raiz ? JS(raiz, "escena3dInicial", "") : std::string());
    if (gFilas.size() > 1)
        w3dLogf("[raices] el proyecto trae %d escena(s)/juego(s)/prefab(s); inicial '%s'", (int)gFilas.size(),
                gInicial.empty() ? gFilas[(size_t)bloque].nombre.c_str() : gInicial.c_str());
}

void W3dRaicesCerrarProyecto() {
    // primero los CONTEXTOS (asi ~Object no los recorre objeto por objeto) y despues los arboles
    for (size_t i = 0; i < gFilas.size(); i++) { BorrarContexto(gFilas[i].ctx); gFilas[i].ctx = NULL; }
    for (size_t i = 0; i < gFilas.size(); i++) {
        if ((int)i == gActiva) continue;
        LiberarRaiz(gFilas[i]);
    }
    // la activa sigue siendo SceneCollection (ReiniciarEscena la vacia); sale del registro
    if (SceneCollection) W3dRaizOlvidar(SceneCollection);
    gFilas.clear(); gActiva = -1; gInicial.clear();
}

void W3dRaicesGuardadas(const std::vector<std::string>& entradas) {
    for (size_t i = 0; i < gFilas.size() && i < entradas.size(); i++) {
        gFilas[i].entrada = entradas[i];
        gFilas[i].cargaFallo = false;
    }
}

// ============================================================================
//  EL JUEGO: cargar otra escena EN la raiz activa (runtime compilado)
// ============================================================================
bool W3dRaizCargarEnActiva(int idx) {
    Asegurar();
    if (idx < 0 || idx >= (int)gFilas.size() || !SceneCollection) return false;
    // lo que apunta al arbol que se va: seleccion, camara, animaciones de escena y el Mix
    DeseleccionarTodo(); ObjSelects.clear(); ObjActivo = NULL;
    for (size_t i = 0; i < SceneAnimations.size(); i++) delete SceneAnimations[i];
    SceneAnimations.clear(); AnimationObjects.clear(); SceneAnimActiva = 0;
    g_mixEscenas.clear(); g_mixEscenaActiva = -1;
    W3dMixEscenasOlvidar();
    if (KindDeObjeto(ActiveAnimKind)) ActiveAnimKind = AnimEsJuego ? 2 : 0;
    ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    // la escena actual se DESTRUYE entera (la raiz queda: la pedida se arma en ella)
    while (!SceneCollection->Childrens.empty()) {
        Object* o = SceneCollection->Childrens.back();
        SceneCollection->Childrens.pop_back();
        W3dLiberarSubarbol(o);
    }
    CollectionActive = SceneCollection;
    ApagarLucesGL();
    if (gActiva >= 0 && gActiva < (int)gFilas.size() && gActiva != idx) {
        gFilas[(size_t)gActiva].raiz = NULL; gFilas[(size_t)gActiva].escena = NULL;
    }
    gFilas[(size_t)idx].raiz = SceneCollection;
    gFilas[(size_t)idx].escena = SceneDe(SceneCollection);
    gActiva = idx;
    const bool ok = W3dRaizLeerContenido(gFilas[(size_t)idx]);
    InitSceneAnimations();
    DeseleccionarTodo(); ObjActivo = NULL;
    w3dLogf("[raices] escena '%s' cargada en la raiz activa%s", gFilas[(size_t)idx].nombre.c_str(),
            ok ? "" : " (NO SE PUDO LEER)");
    g_redraw = true;
    return ok;
}

// ============================================================================
//  cambiarEscena() CON ESCENAS 3D (el pedido llega de W3dEscena, que primero mira las UI)
// ============================================================================
static W3dRaizCambioJuegoFn gCambioJuego = 0;
static int gPedido = -1;
void W3dRaicesSetCambioJuego(W3dRaizCambioJuegoFn fn) { gCambioJuego = fn; gPedido = -1; }

static bool PedirEscena3D(const std::string& nombre, bool reiniciar) {
    (void)reiniciar;   // una escena/juego 3D pedido SIEMPRE arranca de cero (como abrir un nivel)
    const int i = W3dRaizBuscar(W3D_RAIZ_ESCENA, nombre);   // (escenas y juegos)
    if (i < 0 || !gCambioJuego) return false;
    gPedido = i;
    return true;
}
static void CineAplicarPendiente();   // (las cinematicas, abajo)
static bool AplicarEscena3D() {
    // una cinematica que termino, un pararEscena3D() o un reproducirEscena() pedidos en este frame
    CineAplicarPendiente();
    if (gPedido < 0 || !gCambioJuego) { gPedido = -1; return false; }
    const int i = gPedido;
    gPedido = -1;
    W3dCineAbortar();   // (un cambiarEscena en medio de una cinematica: se deja la cinematica sin su alTerminar)
    gCambioJuego(i);
    return true;
}

// ---- la CINEMATICA de una ESCENA pedida jugando ----
int W3dRaizCinematica() {
    if (SceneAnimations.empty()) return -1;
    // la ANIMACION DE ESCENA elegida (un clip de objeto no es "la" animacion de la raiz: si lo que se
    // estaba editando era uno, la primera animacion de escena)
    if (SceneAnimActiva >= 0 && SceneAnimActiva < (int)SceneAnimations.size() && !W3dAnimEsClip(SceneAnimActiva))
        return SceneAnimActiva;
    for (size_t i = 0; i < SceneAnimations.size(); i++) if (!W3dAnimEsClip((int)i)) return (int)i;
    return -1;
}
bool W3dRaizArrancarCinematica() {
    if (W3dRaizTipoActiva() != W3D_RAIZ_ESCENA) return false;   // un juego simula; un prefab no se juega
    const int a = W3dRaizCinematica();
    if (a < 0 || !W3dAnimEscenaPlay(a, false)) return false;
    W3dAnimEscenaTick(0.0f);   // el cuadro de inicio ya posado (los scripts de su inicio() lo ven)
    w3dLogf("[raices] escena '%s': reproduce su animacion '%s' desde el frame %d",
            gActiva >= 0 ? gFilas[(size_t)gActiva].nombre.c_str() : "?", SceneAnimations[(size_t)a]->name.c_str(),
            SceneAnimations[(size_t)a]->startFrame);
    return true;
}

// ============================================================================
//  CINEMATICAS: reproducirEscena() (ver W3dRaices.h). La ESCENA pasa a ser la raiz
//  activa con W3dRaizUsar (el juego guarda su contexto y su arbol queda intacto,
//  fuera del arbol activo: nada suyo corre) y vuelve con el mismo swap.
// ============================================================================
namespace {
struct W3dCineEstado {
    bool activa;
    int escena, juego;            // filas del registro
    Object* llamador;             // el objeto cuyo script la pidio (lo suelta ~Object)
    std::string fn;               // su funcion global a llamar al volver ("" = ninguna)
    bool cargadaPara;             // la escena no estaba cargada: se cargo para esto (el runtime la libera)
    bool termino;                 // su animacion llego al Fin (se vuelve al final del frame)
    W3dAnimEscenaEstado sonaba;   // la animEscena() que sonaba en el juego (sigue al volver)
    W3dCineEstado() : activa(false), escena(-1), juego(-1), llamador(0), cargadaPara(false), termino(false) {}
};
struct W3dCinePedido {
    bool hay, parar;
    int escena;
    Object* llamador;
    std::string fn;
    W3dCinePedido() : hay(false), parar(false), escena(-1), llamador(0) {}
};
}
static W3dCineEstado gCine;
static W3dCinePedido gCinePedido;
static W3dRaizCineFn gCineFn = 0;

void W3dRaicesSetCine(W3dRaizCineFn fn) { gCineFn = fn; gCinePedido = W3dCinePedido(); }
bool W3dCineActiva() { return gCine.activa; }
int  W3dCineEscena() { return gCine.activa ? gCine.escena : -1; }
int  W3dCineJuego()  { return gCine.activa ? gCine.juego : -1; }

// la vuelta al juego (con o sin su alTerminar)
static void CineVolver(bool llamar) {
    if (!gCine.activa) return;
    W3dCineEstado c = gCine;
    gCine = W3dCineEstado();
    // la ESCENA todavia es la activa: el build descarga sus scripts y la devuelve a como estaba
    if (gCineFn) gCineFn(W3D_CINE_SALE);
    W3dAnimEscenaReset();
    // el modo del timeline de la escena: el que tenia (lo que se le forzo para jugarla no es suyo)
    if (c.escena >= 0 && c.escena < (int)gFilas.size() && W3dRaizActiva() == c.escena) {
        ActiveAnimKind = 0; AnimEsJuego = false;
    }
    if (c.juego >= 0 && c.juego < (int)gFilas.size() && W3dRaizUsar(c.juego)) {
        ApagarLucesGL();
        W3dAnimEscenaReponerEstado(c.sonaba);   // la animEscena() del juego sigue donde iba
    } else w3dLogfE("[raices] cinematica: no pude volver al juego (fila %d)", c.juego);
#ifdef W3D_SIN_EDITOR
    // el juego compilado no guarda lo que no usa: una escena cargada solo para esto se libera
    if (c.cargadaPara && c.escena >= 0 && c.escena < (int)gFilas.size() && c.escena != gActiva)
        LiberarRaiz(gFilas[(size_t)c.escena]);
#endif
    w3dLogf("[raices] cinematica '%s' %s: vuelve el juego '%s'%s%s",
            (c.escena >= 0 && c.escena < (int)gFilas.size()) ? gFilas[(size_t)c.escena].nombre.c_str() : "?",
            llamar ? "terminada" : "cortada",
            (gActiva >= 0 && gActiva < (int)gFilas.size()) ? gFilas[(size_t)gActiva].nombre.c_str() : "?",
            (llamar && c.llamador && !c.fn.empty()) ? ", llama " : "", (llamar && c.llamador) ? c.fn.c_str() : "");
    g_redraw = true;
    // alTerminar() en los scripts del objeto que la pidio (ya en el juego: puede pedir la siguiente)
    if (llamar && c.llamador && !c.fn.empty()) W3dScriptEvento(c.llamador, c.fn.c_str(), (Object* const*)NULL, 0, NULL, 0);
}

static bool CineEmpezar(int idx, Object* llamador, const std::string& fn) {
    const int juego = gActiva;
    if (idx < 0 || idx >= (int)gFilas.size() || idx == juego) return false;
    const bool cargada = (gFilas[(size_t)idx].raiz != NULL);
    if (!cargada && !W3dRaizCargar(idx)) return false;
    W3dCineEstado c;
    c.escena = idx; c.juego = juego; c.llamador = llamador; c.fn = fn; c.cargadaPara = !cargada;
    // lo que sonaba en el juego se deja en pausa (se retoma al volver)
    W3dAnimEscenaGuardarEstado(&c.sonaba);
    W3dAnimEscenaReset();
    if (!W3dRaizUsar(idx)) { W3dAnimEscenaReponerEstado(c.sonaba); return false; }
    ApagarLucesGL();
    // se JUEGA (no se edita): el timeline queda en "Juego" mientras dure (el editor sigue llamando al tick
    // de la partida y los esqueletos reproducen sus clips con su cabezal, como en el juego compilado)
    ActiveAnimKind = 2; AnimEsJuego = true;
    ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    gCine = c;
    gCine.activa = true;
    if (gCineFn) gCineFn(W3D_CINE_ANTES);
    // su animacion de escena, de su Inicio a su Fin (una pasada)
    const int a = W3dRaizCinematica();
    if (a >= 0 && W3dAnimEscenaPlay(a, false)) {
        W3dAnimEscenaTick(0.0f);   // el cuadro de inicio ya posado (sus scripts lo ven en inicio())
        CurrentFrame = W3dAnimEscenaFrame();
    }
    if (gCineFn) gCineFn(W3D_CINE_ENTRO);
    w3dLogf("[raices] cinematica: el juego '%s' queda en pausa y se reproduce la escena '%s' (%s, frames %d..%d)",
            (juego >= 0 && juego < (int)gFilas.size()) ? gFilas[(size_t)juego].nombre.c_str() : "?",
            gFilas[(size_t)idx].nombre.c_str(), a >= 0 ? SceneAnimations[(size_t)a]->name.c_str() : "sin animacion",
            a >= 0 ? SceneAnimations[(size_t)a]->startFrame : 0, a >= 0 ? SceneAnimations[(size_t)a]->endFrame : 0);
    g_redraw = true;
    return true;
}

void W3dCineTick(float dt) {
    if (!gCine.activa) return;
    W3dAnimEscenaTick(dt);
    if (W3dAnimEscenaFrame() >= 0) CurrentFrame = W3dAnimEscenaFrame();   // (el cabezal del timeline la sigue)
    if (W3dAnimEscenaTermino()) gCine.termino = true;
}

void W3dCineAbortar() {
    gCinePedido = W3dCinePedido();
    CineVolver(false);
}

// al final del frame (W3dEscenaAplicarPendiente): primero la vuelta (termino / pararEscena3D) con su
// alTerminar, y despues un reproducirEscena() pedido (tambien el que pidio ese alTerminar: encadenar)
static void CineAplicarPendiente() {
    if (gCine.activa && (gCine.termino || gCinePedido.parar)) {
        gCinePedido.parar = false;
        CineVolver(true);
    }
    gCinePedido.parar = false;
    if (gCinePedido.hay) {
        const W3dCinePedido p = gCinePedido;
        gCinePedido = W3dCinePedido();
        // pedida desde ADENTRO de otra cinematica: la anterior se deja (sin su alTerminar)
        if (gCine.activa) CineVolver(false);
        if (!CineEmpezar(p.escena, p.llamador, p.fn))
            w3dLogfE("[raices] reproducirEscena: no pude reproducir '%s'",
                     (p.escena >= 0 && p.escena < (int)gFilas.size()) ? gFilas[(size_t)p.escena].nombre.c_str() : "?");
    }
}

static void CorrerIndices(int desde, int delta) {
    // salio una fila: la que la nombraba ya no nombra nada (no puede ser la activa ni la de una cinematica:
    // PuedeBorrarse); entro una: los de ahi en adelante se corren uno
    int* idx[5] = { &gActiva, &gPedido, &gCine.escena, &gCine.juego, &gCinePedido.escena };
    for (int k = 0; k < 5; k++) {
        int& i = *idx[k];
        if (i < 0) continue;
        if (delta < 0 && i == desde) i = -1;
        else if (i >= desde + (delta < 0 ? 1 : 0)) i += delta;
    }
}

int W3dRaicesDescargar(const std::vector<int>& filas) {
    int n = 0;
    for (size_t k = 0; k < filas.size(); k++) {
        const int i = filas[k];
        if (i < 0 || i >= (int)gFilas.size() || i == gActiva) continue;
        W3dRaizFila& f = gFilas[(size_t)i];
        if (!f.raiz || f.bloque || f.entrada.empty() || f.cargaFallo) continue;
        if (W3dCineActiva() && (i == gCine.escena || i == gCine.juego)) continue;
        LiberarRaiz(f);
        n++;
    }
    if (n) w3dLogf("[raices] %d raiz(es) que se cargaron para guardar vuelven a quedar sin cargar", n);
    return n;
}

// ---- los binds (W3dEscena los registra; ver W3dEscenaSetCine) ----
static bool CinePedirBind(const std::string& nombre, Object* llamador, const std::string& fn) {
    if (!gCineFn) return false;   // (sin partida: el editor fuera del Play)
    const int i = W3dRaizBuscar(W3D_RAIZ_ESCENA, nombre);
    if (i < 0) { w3dLogfW("reproducirEscena(): no hay una escena '%s'", nombre.c_str()); return false; }
    if (W3dRaizTipoDe(i) != W3D_RAIZ_ESCENA) {
        w3dLogfW("reproducirEscena(): '%s' es un juego, no una escena (para ir a otro juego: cambiarEscena)", nombre.c_str());
        return false;
    }
    const int juego = gCine.activa ? gCine.juego : gActiva;
    if (i == juego) { w3dLogfW("reproducirEscena(): '%s' es la raiz que esta corriendo", nombre.c_str()); return false; }
    gCinePedido.hay = true; gCinePedido.parar = false;
    gCinePedido.escena = i; gCinePedido.llamador = llamador; gCinePedido.fn = fn;
    return true;
}
static void CinePararBind() { if (gCine.activa) gCinePedido.parar = true; }
static const char* CineActualBind() {
    return (gCine.activa && gCine.escena >= 0 && gCine.escena < (int)gFilas.size()) ? gFilas[(size_t)gCine.escena].nombre.c_str() : "";
}

// ============================================================================
//  ~Object: lo que el contexto de una raiz INACTIVA guarda de un objeto que se borra
//  (la seleccion, la camara, sus luces, las curvas de sus animaciones de escena)
// ============================================================================
static void QuitarDeCurvas(std::vector<AnimationObject>& v, Object* o) {
    for (size_t i = v.size(); i-- > 0; ) if (v[i].obj == o) v.erase(v.begin() + (long)i);
}
static void DesvincularContextos(Object* o) {
    for (size_t k = 0; k < gFilas.size(); k++) {
        W3dContextoRaiz* c = gFilas[k].ctx;
        if (!c) continue;
        for (size_t i = c->selects.size(); i-- > 0; ) if (c->selects[i] == o) c->selects.erase(c->selects.begin() + (long)i);
        if (c->activo == o) c->activo = NULL;
        if (c->coleccion == o) c->coleccion = NULL;
        if ((Object*)c->camara == o) c->camara = NULL;
        for (size_t i = c->luces.size(); i-- > 0; ) if ((Object*)c->luces[i] == o) c->luces.erase(c->luces.begin() + (long)i);
        if ((Object*)c->animArm == o || (Object*)c->animMesh == o) { c->animArm = NULL; c->animMesh = NULL; c->animKind = 0; }
        QuitarDeCurvas(c->curvas, o);
        for (size_t e = 0; e < c->escenas.size(); e++) {
            if (!c->escenas[e]) continue;
            QuitarDeCurvas(c->escenas[e]->objetos, o);
            if (c->escenas[e]->esClip && c->escenas[e]->duenio == o) c->escenas[e]->duenio = NULL;   // clip huerfano
        }
    }
    // el que pidio una cinematica ya no esta: al volver no hay a quien llamar
    if (gCine.llamador == o) gCine.llamador = NULL;
    if (gCinePedido.llamador == o) gCinePedido.llamador = NULL;
}

namespace {
struct RegistrarGanchos {
    RegistrarGanchos() {
        W3dDesvincularRegistrar(DesvincularContextos);
        W3dEscenaSet3D(PedirEscena3D, AplicarEscena3D);
        W3dEscenaSetCine(CinePedirBind, CinePararBind, CineActualBind);
    }
} gRegistrarGanchos;
}
