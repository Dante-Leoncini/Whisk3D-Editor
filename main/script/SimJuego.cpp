// ============================================================================
//  SimJuego.cpp — ver SimJuego.h.
// ============================================================================
#include "script/SimJuego.h"
#include "script/W3dScript.h"
#include "script/BindsJuego.h"        // alimentar el estado de los binds de juego (apretado/etc) en el Play
#include "physics/W3dFisica.h"        // paso de fisica del Core (el MISMO que corre el juego compilado)
#include "W3dEscena.h"                // motor multi-escena COMPARTIDO (activa / init perezoso / cambio pendiente)
#include "objects/Objects.h"
#include "objects/Light.h"            // el snapshot guarda/restaura el color difuso (setColor/setEnergia)
#include "objects/Camera.h"           // idem el RIEL de la camara (setRiel/setRielNodo son estado de juego)
#include "objects/UI.h"
#include "objects/Elemento2D.h"
#include "objects/Texto2D.h"
#include "render/UIOverlay.h"
#include "render/OpcionesRender.h"     // g_redraw
#include "animation/Animation.h"       // CurrentFrame / StartFrame / EndFrame / PlayAnimation
#include "objects/Gamepad.h"           // axisState / buttonState (el pad SDL del editor)
#include "objects/Mesh.h"              // Mesh::animations (la vertex anim tambien es estado de partida)
#include "animation/VertexAnimation.h" // FindTargetAnim / EvalVertexAnim / UpdateAnimations
#include "ViewPorts/Editor2D.h"        // SimToquePantalla: mapeo pantalla->lienzo real
#include "ViewPorts/ViewPort3D.h"      // idem para el 3D: el rect real del HUD (hudX0/hudEsc)
#ifndef W3D_SYMBIAN
#include <SDL.h>   // SDL_CONTROLLER_* / SDLK_*: solo el input del Play de escritorio
#endif
#include <vector>
#include <string>

// ---- SNAPSHOT del estado (lo que un script puede tocar hoy) ----------------
// TIENE que cubrir TODO lo que la API lua puede escribir: lo que no se guarde aca queda modificado
// para siempre en la escena del usuario cuando aprieta Stop (Aplicar restaura solo lo que hay).
// Por eso ademas de la posicion van la rotacion (quaternion + los dos displays), la escala, la
// visibilidad y el color de las luces.
struct SimEnt {
    Object* o;
    float px, py, pz;
    Quaternion rot;
    Vector3 rotEuler;      // el euler CON sus vueltas (el quaternion no distingue 0 de 360)
    float rotAngle;        // display axis-angle
    Vector3 rotAxis;
    Vector3 scale;
    bool  vis, renderiz;
    float rot2d, ancho, alto, scrollX, scrollY, opacidad;
    bool  es2d, esTexto, esLuz;
    float dif[4];          // solo si esLuz: el color difuso (lo tocan setColor/setEnergia)
    // FISICA: la velocidad tambien es estado del juego. Sin guardarla, el viaje en el tiempo
    // (SimStep hacia atras / SimIrA) reponia las posiciones pero la pelota seguia con la
    // velocidad de AHORA. Solo se guarda si el objeto tiene cuerpo (casi ninguno lo tiene).
    bool  tieneVel;
    float vx, vy, vz;
    // EL RIEL de una Camera: setRiel/setRielNodo lo mueven EN JUEGO (un cambio de sala salta
    // entre tres rieles), asi que es estado de partida igual que la velocidad. Sin esto, Stop
    // dejaba la camara del editor pegada al riel del bonus y el viaje en el tiempo reponia las
    // posiciones pero no el riel con el que se habian calculado.
    bool  esCam;
    Curve* riel;
    int   offRiel;
    float rielNodo;
    bool  miradaRiel;   // modo de mirada: setMiradaRiel() lo cambia EN JUEGO
    // VERTEX ANIMATION: el frame que muestra el personaje (y cada enemigo) TAMBIEN es
    // estado de partida. Reporte del dueno, textual: "el timeline la captura de
    // estados del whisk3d editor no tiene en cuenta la animacion del vertex
    // animation del personaje y los enemigos". Sin esto, el scrub / el paso atras reponian
    // las POSICIONES y dejaban la malla en la pose del ULTIMO frame simulado:
    // el personaje retrocedia por el nivel congelado en media zancada.
    // Se guardan los tres campos del reproductor (VertexAnimationActive):
    //   vaPlay = playFrame, la cabeza lectora en cuadros (float, interpolable);
    //   vaAnim = currentAnim, que clip esta sonando;
    //   vaProx = nextAnim, el clip que pidio animar() de lua y todavia no entro.
    // Restaurar los tres no alcanza: la POSE vive en mesh->vertex[]/normals[]/uv[],
    // asi que Aplicar ademas re-evalua (EvalVertexAnim) y reescribe la geometria.
    bool  tieneVA;
    float vaPlay;
    int   vaAnim, vaProx;
    std::string texto;
};
struct SimSnap { std::vector<SimEnt> ents; };

static bool gActiva = false;
static SimSnap gBase;        // frame 0 del juego (post-inicio(): la base del grab/rewind)
static SimSnap gBaseEditor;  // la escena DEL USUARIO antes de tocar nada (la restaura Stop)
static std::vector<SimSnap> gGrab;    // un snapshot por frame simulado (editor)
static int gTick = 0;
static std::vector<Object*> gScripted;
static int gGrabOffset = 0;   // tick ABSOLUTO del primer snapshot (el cache rueda)
int gSimCacheMax = 250;       // techo del cache (frames), configurable (tarjeta Juego)
bool gSimCacheOn = true;      // cache de juego (rewind) ON/OFF (checkbox tarjeta Juego). OFF = sin snapshot -> fluido
// FLECHAS DEL TECLADO apretadas AHORA [arriba, abajo, izquierda, derecha]. Las
// mantiene SimTeclaSDL y las mezcla TickReal en el stick izquierdo, igual que el
// d-pad. Se sueltan al parar la partida (SimStop) para que una flecha que quedo
// apretada al cortar no arrastre al jugador en la partida siguiente.
static bool gFlecha[4] = { false, false, false, false };

bool SimActiva() { return gActiva; }
// modo juego DE VERDAD (ver SimJuego.h): timeline en "Juego" + partida cargada.
// Gate compartido de los overlays de edicion / chrome que el juego no muestra.
bool W3dJuegoCorriendo() { extern bool AnimEsJuego; return AnimEsJuego && gActiva; }
int  SimFramesGrabados() { return (int)gGrab.size(); }
int  SimFrameActual() { return gTick; }
int  SimPrimerFrame() { return gGrabOffset; }

static void Recorrer(Object* o, SimSnap* snap, std::vector<Object*>* scripted) {
    if (!o) return;
    if (snap) {
        SimEnt e;
        e.o = o;
        e.px = o->pos.x; e.py = o->pos.y; e.pz = o->pos.z;
        e.rot = o->Rot(); e.rotEuler = o->rotEuler; e.rotAngle = o->rotAngle; e.rotAxis = o->rotAxis;
        e.scale = o->scale;
        e.vis = o->visible; e.renderiz = o->renderizable;
        e.esLuz = (o->getType() == ObjectType::light);
        if (e.esLuz) for (int k = 0; k < 4; k++) e.dif[k] = ((Light*)o)->diffuse[k];
        else         for (int k = 0; k < 4; k++) e.dif[k] = 0.0f;
        e.esCam = (o->getType() == ObjectType::camera);
        e.riel = NULL; e.offRiel = 0; e.rielNodo = -1.0f; e.miradaRiel = false;
        if (e.esCam) {
            Camera* c = (Camera*)o;
            e.riel = c->Riel; e.offRiel = c->offsetRiel; e.rielNodo = c->rielNodo;
            e.miradaRiel = c->usarRotacionDelRiel;
        }
        e.vx = e.vy = e.vz = 0.0f;
        e.tieneVel = W3dFisicaTiene(o);
        if (e.tieneVel) W3dFisicaGetVel(o, &e.vx, &e.vy, &e.vz);
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
        // VERTEX ANIMATION: el reproductor de la malla (ver el comentario de SimEnt)
        e.tieneVA = false; e.vaPlay = 0.0f; e.vaAnim = -1; e.vaProx = -1;
        if (o->getType() == ObjectType::mesh) {
            VertexAnimationActive* va = FindTargetAnim((Mesh*)o);
            if (va) { e.tieneVA = true; e.vaPlay = va->playFrame;
                      e.vaAnim = va->currentAnim; e.vaProx = va->nextAnim; }
        }
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
        // rotacion: se restauran el quaternion Y los displays TAL CUAL estaban (la puerta de SNAPSHOT
        // no re-deriva el euler del quaternion: eso se comeria las vueltas).
        o->SetRotSnapshot(e.rot, e.rotEuler);
        o->scale = e.scale;
        o->visible = e.vis; o->renderizable = e.renderiz;
        if (e.esLuz) for (int k = 0; k < 4; k++) ((Light*)o)->diffuse[k] = e.dif[k];
        if (e.esCam) {
            // el par (Riel, RielName) se repone junto: RielName es lo que guarda el .w3d
            Camera* c = (Camera*)o;
            c->Riel = e.riel;
            c->RielName = e.riel ? e.riel->name : std::string();
            c->offsetRiel = e.offRiel;
            c->rielNodo = e.rielNodo;
            c->usarRotacionDelRiel = e.miradaRiel;
        }
        // la velocidad vuelve a la que tenia en ese frame (solo si ya tenia cuerpo: no se
        // le inventa uno a un objeto que en ese momento no se movia)
        if (e.tieneVel) W3dFisicaSetVel(o, e.vx, e.vy, e.vz);
        if (e.es2d) {
            Elemento2D* el = (Elemento2D*)o;
            el->rot2d = e.rot2d; el->ancho = e.ancho; el->alto = e.alto;
            el->scrollX = e.scrollX; el->scrollY = e.scrollY; el->opacidad = e.opacidad;
        } else if (o->getType() == ObjectType::ui) {
            ((UI*)o)->ancho = e.ancho; ((UI*)o)->alto = e.alto;
        }
        if (e.esTexto) ((Texto2D*)o)->texto = e.texto;
        // VERTEX ANIMATION: reponer la cabeza lectora Y VOLVER A POSAR la malla.
        // Reponer playFrame solo no se ve: la pose vive en mesh->vertex[], que la
        // escribio EvalVertexAnim en el frame que se esta descartando. Por eso se
        // re-evalua aca -- es lo que hace que el scrub del timeline muestre el
        // frame de animacion que corresponde y no la ultima pose simulada.
        if (e.tieneVA) {
            Mesh* m = (Mesh*)o;
            VertexAnimationActive* va = FindTargetAnim(m);
            if (va) {
                va->playFrame   = e.vaPlay;
                va->currentAnim = e.vaAnim;
                va->nextAnim    = e.vaProx;
                if (e.vaAnim >= 0 && e.vaAnim < (int)m->animations.size() && m->animations[e.vaAnim] &&
                    !m->animations[e.vaAnim]->frames.empty()) {
                    m->animations[e.vaAnim]->target = m;   // las anims del .w3d llegan con target NULL
                    EvalVertexAnim(*m->animations[e.vaAnim], m, e.vaPlay);
                }
            }
        }
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

// init de UN subarbol: carga+resuelve+inicio() de cada objeto con script (en orden de
// arbol). reiniciar=true descarga antes (recrea los lua_State desde cero).
static void EditorInitSubtree(Object* o, bool reiniciar) {
    if (!o) return;
    if (o->scriptDatos && !o->scriptDatos->scripts.empty()) {
        if (reiniciar) W3dScriptDescargarDe(o);
        if (W3dScriptCargar(o)) {
            SimReresolver(o);      // referencias (por nombre) y opciones, de TODOS sus scripts
            W3dScriptInicio(o);
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        EditorInitSubtree(o->Childrens[i], reiniciar);
}
// callback de init de UNA escena (lo registra W3dEscenaSetInit): el motor multi-escena
// lo llama al arrancar la inicial y al hacer el init perezoso de cada escena destino.
static void EditorInitEscena(UI* u, bool reiniciar) { EditorInitSubtree((Object*)u, reiniciar); }

static void SimPlay() {
    // volcar a disco/overlay los .lua editados en el IDE (sucios) ANTES de leerlos: sino Play corre el .lua VIEJO
    // (el cambio vivia solo en el buffer del IDE). Asi cambiar un valor en el script y dar Play YA se ve.
    { extern void IDEGuardarSucios(); IDEGuardarSucios(); }
    gScripted.clear();
    Recorrer(SceneCollection, NULL, &gScripted);   // en el orden del arbol (outliner)
    if (gScripted.empty()) return;
    // snapshot de la escena DEL USUARIO (antes de que inicio() toque nada): es lo
    // que Stop restaura. (Antes se pisaba con el snapshot post-inicio de abajo y
    // Stop dejaba pegado lo que los scripts movieron en inicio().)
    Snapshot(&gBaseEditor);
    // MULTI-ESCENA: registrar las escenas UI. La INICIAL arranca ya; las demas quedan
    // cargadas pero sin inicir (init perezoso al hacer cambiarEscena()).
    W3dEscenaSetInit(EditorInitEscena);
    W3dEscenaRegistrarTodas();
    bool hayEscenas = W3dEscenaHayEscenas();
    // los objetos con script que NO cuelgan de ninguna escena UI (3D, gamepad, etc.) NO
    // son parte del sistema de escenas: se inician SIEMPRE (comportamiento viejo). Los de
    // escenas UI los inicia la escena (la inicial ahora; el resto, perezoso).
    for (size_t i = 0; i < gScripted.size(); i++) {
        Object* s = gScripted[i];
        Object* raiz = W3dEscenaRaizDe(s);
        bool enEscena = hayEscenas && raiz && raiz->getType() == ObjectType::ui;
        if (enEscena) continue;
        if (!W3dScriptCargar(s)) continue;
        SimReresolver(s);
        W3dScriptInicio(s);
    }
    if (hayEscenas) W3dEscenaArrancar();   // muestra + inicia SOLO la escena inicial
    gGrab.clear();
    gGrabOffset = 0;
    // frame 0 del cache (post-inicio): SOLO si el cache esta ON y NO es un juego compilado. Con el cache OFF (o
    // g_modoJuego) gGrab queda VACIO -> juego fluido y SIN rewind. Es CLAVE dejarlo vacio: si gGrab tuviera este
    // unico frame 0, SimStep back accederia gGrab[gTick] (gTick avanza pero gGrab no crece) fuera de rango = crash.
    { extern bool g_modoJuego;
      if (gSimCacheOn && !g_modoJuego) { Snapshot(&gBase); gGrab.push_back(gBase); } }
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
            // refs POR ESCENA: buscar dentro de la raiz de la escena del dueno (no en toda
            // SceneCollection) para que no colisionen nombres repetidos entre escenas.
            Object* obj = BuscarPorNombre(W3dEscenaRaizDe(o), val);
            if (obj) W3dScriptResolverRef(o, (int)e, prop, obj);
            W3dScriptResolverOpcion(o, (int)e, prop, val);
            // el MISMO par sirve para las propiedades de VALOR (tipo 2): el que
            // corresponda lo decide el .lua al leer (objeto/opcion/propiedad)
            W3dScriptResolverValor(o, (int)e, prop, val);
        }
    }
}

static void TickReal(float dt) {
    // FISICA del Core (velocidad + rebotes): PRIMERO se integran las posiciones y despues
    // corren los scripts, asi el script ve la posicion nueva y resuelve el choque (rebotar)
    // en el MISMO frame. Es el mismo orden y el mismo dt que el runtime compilado
    // (W3dGameActualizar en game/w3drun.cpp) -> el juego se comporta igual en los dos lados.
    W3dFisicaPaso(dt);
    // CAMARAS CON RIEL: el indice fraccionario (rielIndice, el 5to valor de rielDe)
    // lo horneaba SOLO el dibujo (Viewport3D::UpdateViewOrbit -> UpdatePosition), asi
    // que en ticks sin render -- harness `simplay`, o el primer tick antes del primer
    // frame -- los scripts leian el indice VIEJO y una VisZona modo curva recortaba
    // con la celda de OTRO nodo (cunas negras en el material de referencia). Se hornea
    // TAMBIEN aca, antes de correr los scripts: rielDe() queda fresco en el tick, con
    // el mismo valor que dejaria un frame dibujado entre tick y tick (UpdatePosition
    // es deterministica e idempotente: el dibujo posterior recalcula lo mismo).
    { extern void W3dCamarasRielTick(); W3dCamarasRielTick(); }
    // el GAMEPAD analogico entra a los scripts (los ejes ya vienen con deadzone).
    // En Symbian el pad SDL no existe (el input del juego entra por otra via).
#ifndef W3D_SYMBIAN
    float lx = axisState[SDL_CONTROLLER_AXIS_LEFTX], ly = axisState[SDL_CONTROLLER_AXIS_LEFTY];
    // ---- EL D-PAD (LA CRUCETA) ENTRA COMO EL STICK IZQUIERDO --------------------
    // Reporte del dueno, textual: "dejame jugar con las flechas del control y no
    // solo el analogico". Los cuatro botones del d-pad YA llegaban a buttonState[]
    // (RefreshInputControllerSDL los guarda como cualquier otro boton) pero nadie se
    // los pasaba a los scripts: el juego solo veia el analogico.
    // Van MEZCLADOS con el eje izquierdo, no como un input aparte, porque el modulo
    // correcto de las 8 direcciones VIVE EN LOS ASSETS: el port ya reconoce "ejes de
    // exactamente 0 o +-1" como cruceta y les aplica su tabla (1.0 en las cardinales,
    // 327/256 en las diagonales, igual que el juego original). Entregandole 0/+-1
    // desde aca, el d-pad anda con el modulo bueno sin tocar un solo script -- y es
    // el MISMO camino por el que ya entran las flechas del teclado.
    // El ANALOGICO MANDA: si esta fuera de la deadzone, la cruceta no pisa nada.
    if (lx == 0.0f && ly == 0.0f) {
        const int dl = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT]  || gFlecha[2]) ? 1 : 0;
        const int dr = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] || gFlecha[3]) ? 1 : 0;
        const int du = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP]    || gFlecha[0]) ? 1 : 0;
        const int dd = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN]  || gFlecha[1]) ? 1 : 0;
        // el eje Y de SDL crece HACIA ABAJO, igual que el del stick: arriba = -1
        lx = (float)(dr - dl); ly = (float)(dd - du);
    }
    W3dScriptStick(0, lx, ly);
    W3dScriptStick(1, axisState[SDL_CONTROLLER_AXIS_RIGHTX], axisState[SDL_CONTROLLER_AXIS_RIGHTY]);
    W3dScriptBotonPad("a", buttonState[SDL_CONTROLLER_BUTTON_A]);
    W3dScriptBotonPad("b", buttonState[SDL_CONTROLLER_BUTTON_B]);
    W3dScriptBotonPad("x", buttonState[SDL_CONTROLLER_BUTTON_X]);
    W3dScriptBotonPad("y", buttonState[SDL_CONTROLLER_BUTTON_Y]);
    // ...y ademas por su nombre, para el script que los quiera como botones sueltos
    // (menus, "pausa con arriba/abajo"). Mismos nombres que las flechas del teclado.
    W3dScriptBotonPad("arriba",     buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP]);
    W3dScriptBotonPad("abajo",      buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN]);
    W3dScriptBotonPad("izquierda",  buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT]);
    W3dScriptBotonPad("derecha",    buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]);
#endif
    for (size_t i = 0; i < gScripted.size(); i++) {
        // solo corren los scripts de la escena ACTIVA (y visibles): un objeto invisible o
        // de una escena inactiva no ejecuta su logica. Sin multi-escena EsDeActiva cae a
        // la regla vieja (solo visible), asi que el 3D/una-sola-UI se comporta igual.
        if (!W3dEscenaEsDeActiva(gScripted[i])) continue;
        W3dScriptActualizar(gScripted[i], dt);
    }
    // SNAPSHOT del estado apretado para el flanco de apretado() del PROXIMO frame (DESPUES de los scripts,
    // igual que el runtime en W3dGameActualizar): asi apretado() funciona identico en el Play y compilado.
    BindsJuegoSnapshotPunteros();
    // el cambio de escena pedido por cambiarEscena() se aplica ACA, al final del frame (no
    // en el callback: no mutar la iteracion de arriba). El init perezoso corre en este punto.
    W3dEscenaAplicarPendiente();
    // VERTEX ANIMS AL AVANZAR FRAME A FRAME EN PAUSA. Jugando, quien las hace
    // correr es el loop de render (main.cpp llama a UpdateAnimations una vez por
    // frame); pero con el timeline EN PAUSA ese loop las congela a proposito
    // ("el mundo del juego no respira si el juego no corre"). Sin esto, el boton
    // ">" del timeline simulaba un frame nuevo -- fisica, scripts, snapshot --
    // con el personaje congelado en la misma pose, y ESA pose quedaba grabada. Solo en
    // pausa, asi que jugando nunca se avanza dos veces.
    if (!PlayAnimation) UpdateAnimations(dt);
    gTick++;
    // CACHE DE JUEGO (rewind): un snapshot de TODA la escena por tick. Es lo que DEGRADA el fps (arranca fluido y
    // cae a ~15 al llenarse el buffer -> clonar la escena + memmove del vector cada frame). Se saltea si el usuario
    // destildo "Cache de juego" (gSimCacheOn=false) o si corre un JUEGO COMPILADO (g_modoJuego: un juego no rebobina,
    // nunca debe cachear). Sin cache, SimTickPlay va directo a TickReal (gGrab vacio -> el branch de futuro grabado
    // de :382 no entra) y SimStep/SimSeek hacen early-return con el buffer vacio.
    { extern bool g_modoJuego;
      if (gSimCacheOn && !g_modoJuego) {
          SimSnap s; Snapshot(&s); gGrab.push_back(s);
          // el cache RUEDA: al llenarse se descartan los frames mas viejos (limite de memoria)
          int techo = (gSimCacheMax > 10) ? gSimCacheMax : 10;
          while ((int)gGrab.size() > techo) { gGrab.erase(gGrab.begin()); gGrabOffset++; }
      } }
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
    { extern void AlimentarDedosJuego(); AlimentarDedosJuego(); } // multi-touch: dedos -> dedo(n)
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

// jugar con el DEDO: el toque de un viewport se pasa al lienzo del juego (px,
// centro 0,0 como posPx) y llega a los scripts via toque()
// mapea un punto de pantalla (mx,my) del viewport 'v' al lienzo del juego (px, centro 0,0)
static void MapearAJuego(ViewportBase* v, int mx, int my, float* cx, float* cy) {
    // (P5) el override que este mapeo necesite es DE ESTE TOQUE, no del juego: al salir
    // se repone lo que habia (el del viewport ACTIVO). Sin esto, un click en el segundo
    // viewport dejaba SU pantalla puesta y los binds del juego (pantalla()) la leian.
    UI2D_OverrideVentanaScope alcance;
    // VISTA DE JUEGO: re-fijar el override del lienzo de ESTE viewport ANTES de leer su tamano,
    // asi el input usa el MISMO lienzo que el dibujo (coherencia con varios viewports 2D)
    Editor2DAlimentarOverride(v);
    // idem para el 3D en modo juego con UI dinamica: su HUD se dibujo con el lienzo
    // overrideado a su pantalla (ver Viewport3D::RenderUI) y el input tiene que verlo
    if (v && v->ViewportKind() == 1) {
        Viewport3D* e3o = (Viewport3D*)v;
        if (e3o->hudOverride && e3o->hudW > 0.5f) UI2D_OverrideVentana(e3o, e3o->hudW, e3o->hudH);
    }
    float lw, lh; UI2D_TamanoLienzo(&lw, &lh);
    *cx = 0; *cy = 0;
    if (v && v->ViewportKind() == 6) {
        Editor2D* e = (Editor2D*)v;                  // Editor2D: mapeo REAL (zoom + pan)
        // lastFx/lastFy/lastEsc son LOCALES al viewport (su Render dibuja en ortho local):
        // el mouse viene en coords de VENTANA -> descontar el origen del viewport (v->x, v->y),
        // igual que hace la rama del 3D. Sin esto, con el viewport 2D fuera de la esquina
        // superior-izquierda el toque del juego caia corrido.
        if (e->lastEsc > 0.0001f) {
            *cx = ((mx - v->x) - e->lastFx) / e->lastEsc - lw * 0.5f;
            *cy = ((my - v->y) - e->lastFy) / e->lastEsc - lh * 0.5f;
        }
    } else if (v && v->ViewportKind() == 1 && ((Viewport3D*)v)->hudW > 0.5f) {
        // Viewport 3D: la pantalla del juego es el RECT DONDE SE DIBUJO EL HUD (el marco
        // del passepartout / letterbox uniforme, ver Viewport3D::RenderUI), no el viewport
        // entero: mismo mapeo que la rama del Editor2D (rect + escala uniforme).
        Viewport3D* e3 = (Viewport3D*)v;
        if (e3->hudEsc > 0.0001f) {
            *cx = ((mx - v->x) - e3->hudX0) / e3->hudEsc - lw * 0.5f;
            *cy = ((my - v->y) - e3->hudY0) / e3->hudEsc - lh * 0.5f;
        }
    } else if (v && v->width > 0 && v->height > 0) { // fallback (aun sin HUD dibujado): rect entero
        *cx = ((mx - v->x) / (float)v->width - 0.5f) * lw;
        *cy = ((my - v->y) / (float)v->height - 0.5f) * lh;
    }
}
void SimToquePantalla(ViewportBase* v, int mx, int my, bool activo) {
    float cx, cy; MapearAJuego(v, mx, my, &cx, &cy);
    W3dScriptToque(cx, cy, activo);   // dedo 0 (compat: mouse / 1 dedo)
    BindsJuegoPunto(0, cx, cy, activo);   // cache en LIENZO para apretado()/etc (igual que el runtime)
}
// MULTI-TOUCH: alimenta el dedo 'i' (0-based) al juego -> dedo(i+1) en lua (2 jugadores tactiles)
void SimToquePantallaDedo(ViewportBase* v, int i, int mx, int my, bool activo) {
    float cx, cy; MapearAJuego(v, mx, my, &cx, &cy);
    W3dScriptDedo(i, cx, cy, activo);
    BindsJuegoPunto(i, cx, cy, activo);
}
// MOUSE-OVER: posicion del mouse (sin clic) al juego -> raton() en lua (resaltar la opcion bajo el cursor)
void SimRatonPantalla(ViewportBase* v, int mx, int my, bool dentro) {
    float cx, cy; MapearAJuego(v, mx, my, &cx, &cy);
    W3dScriptRaton(cx, cy, dentro);
    // cache en LIENZO para el overlay (borde de hover de los botones), igual que el runtime
    BindsJuegoRaton(cx, cy, dentro);
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
    if (gGrab.empty()) return true;   // sin cache (destildado o juego compilado): no hay rewind -> no-op
                                      // (evita gGrab[gTick] fuera de rango: gTick avanza pero gGrab no crece)
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
    Aplicar(gBaseEditor);   // la escena del usuario, NO el frame 0 post-inicio()
    W3dScriptDescargarTodo();
    W3dScriptSoltarTeclas();
    gFlecha[0] = gFlecha[1] = gFlecha[2] = gFlecha[3] = false;  // flechas del teclado
    BindsJuegoResetPunteros();   // soltar el dedo/mouse: que no quede un toque viejo para la proxima partida
    W3dEscenaLimpiar();          // soltar el mapa de escenas / activa (el editor vuelve a elegir UI por ObjActivo)
    gGrab.clear();
    gScripted.clear();
    gTick = 0;
    gGrabOffset = 0;
    gActiva = false;
    CurrentFrame = StartFrame;
    g_redraw = true;
}

// hay frames grabados para rebobinar? (con el cache de juego OFF gGrab queda vacio: no hay rewind)
bool SimHayCache() { return !gGrab.empty(); }

// corta/reinicia el cache LIMPIO desde el tick actual. Lo llama el checkbox "Cache de juego": al destildarlo a
// mitad de partida no quedan frames "fantasma" (banda de cache / rewind sobre frames viejos) hasta el proximo Stop.
void SimCacheReset() {
    gGrab.clear();
    gGrabOffset = gTick;
}

// ---- teclado del juego -----------------------------------------------------
void SimTeclaSDL(int sdlk, bool down) {
#ifdef W3D_SYMBIAN
    (void)sdlk; (void)down;   // en Symbian el teclado del juego entra por otra via (keypad propio)
#else
    const char* n = NULL;
    char letra[2] = { 0, 0 };
    // LAS FLECHAS DEL TECLADO tambien mueven el stick izquierdo (ver gFlecha arriba):
    // sin esto un juego que solo lee stick("izq") -- lo normal -- no se podia jugar
    // con el teclado, y cada script tenia que reimplementar el cruce flechas->ejes.
    if      (sdlk == SDLK_UP)    gFlecha[0] = down;
    else if (sdlk == SDLK_DOWN)  gFlecha[1] = down;
    else if (sdlk == SDLK_LEFT)  gFlecha[2] = down;
    else if (sdlk == SDLK_RIGHT) gFlecha[3] = down;
    if (sdlk >= SDLK_a && sdlk <= SDLK_z) { letra[0] = (char)('a' + (sdlk - SDLK_a)); n = letra; }
    else if (sdlk == SDLK_UP)     n = "arriba";
    else if (sdlk == SDLK_DOWN)   n = "abajo";
    else if (sdlk == SDLK_LEFT)   n = "izquierda";
    else if (sdlk == SDLK_RIGHT)  n = "derecha";
    else if (sdlk == SDLK_SPACE)  n = "espacio";
    else if (sdlk == SDLK_RETURN || sdlk == SDLK_KP_ENTER) n = "enter";
    else if (sdlk >= SDLK_0 && sdlk <= SDLK_9) { letra[0] = (char)('0' + (sdlk - SDLK_0)); n = letra; }
    if (n) W3dScriptTecla(n, down);
#endif
}
