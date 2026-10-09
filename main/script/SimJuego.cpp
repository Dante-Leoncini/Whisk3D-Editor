// ============================================================================
//  SimJuego.cpp — ver SimJuego.h.
// ============================================================================
#include "animation/SkeletalAnimation.h" // W3dArmaturesJuegoTick
#include "script/SimJuego.h"
#include "script/W3dScript.h"
#include "script/BindsJuego.h"        // alimentar el estado de los binds de juego (apretado/etc) en el Play
#include "physics/W3dFisica.h"        // paso de fisica del Core (el MISMO que corre el juego compilado)
#include "physics/W3dRigido.h"        // cuerpos rigidos del Core (cajas + impulsos + contactos)
#include "physics/W3dHitbox.h"        // hitbox del Core (alEntrar/alQuedarse/alSalir + alTocar)
#include "W3dEscena.h"                // motor multi-escena COMPARTIDO (activa / init perezoso / cambio pendiente)
#include "W3dRaices.h"                // cambiarEscena() a otra ESCENA 3D del proyecto (el Stop vuelve a la de antes)
#include "io/Streaming.h"             // las instancias DIFERIDAS cargan/descargan jugando (el Stop las repone)
#include "w3dlog.h"
#include "audio/W3dAudio.h"           // Parar = cortar TODAS las voces; Play = despausar el mixer
#include "objects/Objects.h"
#include "objects/Armature.h"         // el estado de JUEGO de los esqueletos (clip, cabezal, capas, transicion)
#include "animation/W3dAnimSet.h"      // las CAPAS de clips de jerarquia de una raiz (objetoCapa las cambia jugando)
#include "objects/Light.h"            // el snapshot guarda/restaura el color difuso (setColor/setEnergia)
#include "objects/Particulas.h"        // idem el activo del emisor (setEmitiendo)
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
#include <map>
#include <set>
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
    float amb[4], spe[4];  // ...y el ambient/specular (los animan las animaciones de escena y los clips de objeto)
    bool  esPart, partActivo;  // emisor de particulas: setEmitiendo() lo apaga en juego
    bool  esNiebla; float niebla[6];  // setNiebla(): densidad, color, inicio, fin
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
    // HITBOX: setHitboxActivo/setHitboxTam de lua lo cambian EN JUEGO (una puerta que se
    // apaga despues de abrirse): Stop tiene que devolverlo como lo dejo el usuario
    bool  esHb, hbActivo;
    float hbTam[3];
    std::string texto;
};
// ESQUELETOS: el estado de JUEGO de cada armature. animClip / animFrame / animVelocidad /
// animTransicion / animCapa de lua lo cambian EN JUEGO, y el clip activo y las capas del mix
// SE GUARDAN en el .w3d: sin esto, Stop dejaba el armature con el clip que habia elegido el
// script (y el siguiente guardado lo escribia en el archivo), y el viaje en el tiempo reponia
// las posiciones pero no el cabezal con el que se habian posado los huesos.
// Va APARTE de SimEnt (un vector por snapshot, solo con los armatures): casi ningun objeto es
// un esqueleto y el cache del rewind guarda un snapshot por tick.
struct SimArm {
    Armature* a;
    int   clip;                       // animActiva
    float frame, vel;                 // juegoFrame, juegoVel
    bool  loop, termino;              // juegoLoop, juegoTermino
    int   retarget;                   // animRetarget de lua (se guarda en el .w3d)
    std::vector<W3dCapaAnim> capas;   // las capas del mix (animCapa las crea y las cambia jugando)
    // la TRANSICION en curso (animTransicion): el tiempo que falta y la pose congelada desde la
    // que se funde. La pose solo se copia si hay una transicion andando (no pesa por tick).
    float transRest, transTotal;
    std::vector<Vector3> transT, transS;
    std::vector<Quaternion> transQ;
};
// lo de los CLIPS DE JERARQUIA de una raiz que el juego cambia y que va al .w3d: su RETARGET propio
// (animRetarget de lua) y sus CAPAS (objetoCapa...). El Stop los devuelve como los dejo el usuario y el viaje en
// el tiempo repone los cabezales de las capas. Solo las raices que tienen algo de eso (casi ningun objeto).
struct SimJer {
    Object* o;
    int retarget;
    std::vector<W3dCapaAnim> capas;
};
struct SimSnap {
    std::vector<SimEnt> ents;
    std::vector<SimArm> arms;
    std::vector<SimJer> jer;
    // las capas del MIX DE ESCENAS (escenaCapa de lua): son globales y tambien van al .w3d
    std::vector<W3dCapaAnim> mixEscenas;
};

static bool gActiva = false;
// true mientras SimPlay ARRANCA la partida (los inicio() de ArrancarRaizActual corren antes de gActiva = true): lo
// que un inicio() crea con instanciar() -un spawner- es de la partida igual (SimObjetoNuevo)
static bool gArrancando = false;
static SimSnap gBase;        // frame 0 del juego (post-inicio(): la base del grab/rewind)
static SimSnap gBaseEditor;  // la escena DEL USUARIO antes de tocar nada (la restaura Stop)
// ESCENAS 3D (W3dRaices.h): cambiarEscena() a otra escena del proyecto cambia la RAIZ activa sin
// destruir nada. La del Play (gRaizPlay) vuelve con Stop; cada otra por la que paso la partida
// guarda SU base (como la dejo el usuario) para que el Stop la restaure y para que volver a ella
// en la misma partida arranque de cero, como en el juego compilado (que la recarga de su entrada).
struct SimBaseRaiz { Object* raiz; SimSnap snap; };
static Object* gRaizPlay = NULL;
static std::vector<SimBaseRaiz> gBasesRaices;
// CINEMATICA (reproducirEscena, W3dRaices.h): la ESCENA que se reproduce como la dejo el usuario (se le
// devuelve al volver) y sus objetos con script (corren mientras dura; los del juego quedan en pausa)
static SimSnap gCineBase;
static std::vector<Object*> gCineScripted;
static std::vector<SimSnap> gGrab;    // un snapshot por frame simulado (editor)
static int gTick = 0;
static std::vector<Object*> gScripted;
// ---- LO QUE UN SCRIPT CREA Y DESTRUYE JUGANDO (instanciar / destruir de lua, io/Prefabs.h) ----
// CREADOS: las raices que nacieron en el Play (por serial: la direccion se recicla). Se BORRAN al Stop: la
// escena del usuario vuelve a ser la de antes del Play.
// DESTRUIDOS: lo del USUARIO que un script destruyo. No se libera: se DESCUELGA (sus scripts, su cuerpo y sus
// pares se van) y el Stop lo vuelve a colgar donde estaba. Lo creado en el Play si se libera de verdad.
// NUEVOS: los objetos con script que nacieron en este tick; entran a gScripted al final (no en medio del bucle
// de actualizar, que recorre gScripted por indice).
struct SimCreado { Object* o; unsigned serial; };
struct SimDestruido { Object* o; unsigned serial; Object* padre; unsigned padreSerial; int indice; };
static std::vector<SimCreado> gCreadosEnPlay;
static std::vector<SimDestruido> gDestruidosEnPlay;
static std::vector<Object*> gScriptedNuevos;
// true mientras SimJuego libera algo que YA saco de sus listas y fotos (destruir() de lo creado jugando, el Stop):
// el gancho del destructor (SimDesvincular) no tiene nada que hacer
static bool gPodando = false;
// INSTRUMENTACION de scripts (para el [PERF] y el overlay de stats): cuantos corrieron este tick, cuantos hay,
// y ms del loop. Los conteos son ints puros (sirven en PC y Symbian); el ms usa W3dNowMs (real en las dos:
// SDL de alta resolucion en PC, NTickCount ~1ms en Symbian).
int    g_luaScriptsActivos = 0;
int    g_luaScriptsTotal   = 0;
double g_luaTickMs         = 0.0;
// ms ACUMULADOS por objeto-con-script (paralelo a gScripted): dice CUAL script
// se come el tiempo, no solo el total. SimLuaPerfTop lo vuelca y lo resetea.
static std::vector<double> gLuaMsAcum;

// Vuelca los 3 scripts que MAS ms acumularon desde el ultimo volcado, como
// "Juego:41 agua:12 camara:2" (promedio POR TICK si frames > 1), y resetea el
// acumulador. Lo consume el [PERF] de Symbian; en PC sirve igual desde el log.
void SimLuaPerfTop(char* buf, int bufLen, int frames) {
    if (!buf || bufLen < 24) return;
    buf[0] = 0;
    if (frames < 1) frames = 1;
    char* p = buf; char* fin = buf + bufLen - 24;   // margen para "nombre:9999 "
    for (int k = 0; k < 3 && p < fin; k++) {
        size_t mejor = gLuaMsAcum.size(); double mms = 0.0;
        for (size_t i = 0; i < gLuaMsAcum.size() && i < gScripted.size(); i++)
            if (gLuaMsAcum[i] > mms) { mms = gLuaMsAcum[i]; mejor = i; }
        if (mejor >= gLuaMsAcum.size() || mms <= 0.0) break;
        char nom[13]; size_t nl = gScripted[mejor]->name.size(); if (nl > 12) nl = 12;
        memcpy(nom, gScripted[mejor]->name.c_str(), nl); nom[nl] = 0;
        p += sprintf(p, "%s%s:%d", k ? " " : "", nom, (int)(mms / frames + 0.5));
        gLuaMsAcum[mejor] = 0.0;
    }
    for (size_t i = 0; i < gLuaMsAcum.size(); i++) gLuaMsAcum[i] = 0.0;
}
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
        if (e.esLuz) for (int k = 0; k < 4; k++) { e.dif[k] = ((Light*)o)->diffuse[k];
                                                    e.amb[k] = ((Light*)o)->ambient[k]; e.spe[k] = ((Light*)o)->specular[k]; }
        else         for (int k = 0; k < 4; k++) e.dif[k] = e.amb[k] = e.spe[k] = 0.0f;
        e.esPart = (o->getType() == ObjectType::particulas);
        e.partActivo = e.esPart ? ((Particulas*)o)->activo : true;
        e.esNiebla = W3dNieblaHook && W3dNieblaHook(o, false, e.niebla, 6);
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
        e.esHb = (o->getType() == ObjectType::hitbox);
        e.hbActivo = true; e.hbTam[0] = e.hbTam[1] = e.hbTam[2] = 0.0f;
        if (e.esHb) {
            const W3dHitboxBase* hb = (const W3dHitboxBase*)o;
            e.hbActivo = hb->activo;
            for (int k = 0; k < 3; k++) e.hbTam[k] = hb->tam[k];
        }
        // VERTEX ANIMATION: el reproductor de la malla (ver el comentario de SimEnt)
        e.tieneVA = false; e.vaPlay = 0.0f; e.vaAnim = -1; e.vaProx = -1;
        if (o->getType() == ObjectType::mesh) {
            VertexAnimationActive* va = FindTargetAnim((Mesh*)o);
            if (va) { e.tieneVA = true; e.vaPlay = va->playFrame;
                      e.vaAnim = va->currentAnim; e.vaProx = va->nextAnim; }
        }
        snap->ents.push_back(e);
        // ESQUELETOS: su estado de juego va aparte (ver SimArm)
        if (o->getType() == ObjectType::armature) {
            Armature* a = (Armature*)o;
            snap->arms.push_back(SimArm());
            SimArm& sa = snap->arms.back();
            sa.a = a;
            sa.clip = a->animActiva;
            sa.frame = a->juegoFrame; sa.vel = a->juegoVel;
            sa.loop = a->juegoLoop; sa.termino = a->juegoTermino;
            sa.retarget = a->retarget;
            sa.capas = a->capas;
            sa.transRest = a->transRestante; sa.transTotal = a->transTotal;
            if (a->transRestante > 0.0f) { sa.transT = a->transT; sa.transS = a->transS; sa.transQ = a->transQ; }
        }
        // RETARGET y CAPAS de clips de jerarquia (ver SimJer)
        if (o->clipsJer && (o->clipsJer->retarget >= 0 || !o->clipsJer->capas.empty())) {
            snap->jer.push_back(SimJer());
            snap->jer.back().o = o;
            snap->jer.back().retarget = o->clipsJer->retarget;
            snap->jer.back().capas = o->clipsJer->capas;
        }
    }
    if (scripted && o->scriptDatos && !o->scriptDatos->scripts.empty())
        scripted->push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        Recorrer(o->Childrens[i], snap, scripted);
}

static void Snapshot(SimSnap* s) {
    s->ents.clear();
    s->arms.clear();
    s->jer.clear();
    Recorrer(SceneCollection, s, NULL);
    s->mixEscenas = g_mixEscenas;
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
        if (e.esLuz) for (int k = 0; k < 4; k++) { ((Light*)o)->diffuse[k] = e.dif[k];
                                                    ((Light*)o)->ambient[k] = e.amb[k]; ((Light*)o)->specular[k] = e.spe[k]; }
        if (e.esPart) ((Particulas*)o)->activo = e.partActivo;
        if (e.esNiebla) { float v[6]; for (int k = 0; k < 6; k++) v[k] = e.niebla[k]; W3dNieblaHook(o, true, v, 6); }
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
        if (e.esHb) {
            W3dHitboxBase* hb = (W3dHitboxBase*)o;
            hb->activo = e.hbActivo;
            for (int k = 0; k < 3; k++) hb->tam[k] = e.hbTam[k];
        }
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
    // ESQUELETOS: el clip, el cabezal, las capas y la transicion de ESE momento
    for (size_t i = 0; i < s.arms.size(); i++) {
        const SimArm& e = s.arms[i];
        Armature* a = e.a; if (!a) continue;
        a->animActiva = e.clip;
        a->juegoFrame = e.frame; a->juegoVel = e.vel;
        a->juegoLoop = e.loop; a->juegoTermino = e.termino;
        if (a->retarget != e.retarget) W3dArmatureRetarget(a, e.retarget);
        a->capas = e.capas;
        a->transRestante = e.transRest; a->transTotal = e.transTotal;
        a->transT = e.transT; a->transS = e.transS; a->transQ = e.transQ;
        // y la POSE se recalcula: el cache de EvaluarPoseEsqueleto mira el frame y el clip,
        // no las capas ni la transicion que se acaban de reponer
        a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->lastPoseFrameF = -999999.0f;
    }
    g_mixEscenas = s.mixEscenas;
    // RETARGET y CAPAS DE CLIPS DE JERARQUIA: los de ESE momento; una raiz que entonces no tenia nada de eso
    // (animRetarget / objetoCapa se lo pusieron despues) vuelve a "el de cada clip" y sin capas
    {
        std::map<Object*, const SimJer*> guardado;
        for (size_t k = 0; k < s.jer.size(); k++) if (s.jer[k].o) guardado[s.jer[k].o] = &s.jer[k];
        for (size_t i = 0; i < s.ents.size(); i++) {
            Object* o = s.ents[i].o;
            if (!o) continue;
            std::map<Object*, const SimJer*>::iterator g = guardado.find(o);
            if (g == guardado.end()) {
                if (!o->clipsJer || (o->clipsJer->retarget < 0 && o->clipsJer->capas.empty())) continue;
                o->clipsJer->retarget = -1;
                o->clipsJer->capas.clear();
            } else {
                if (!o->clipsJer) o->clipsJer = new W3dJerRaiz();
                o->clipsJer->retarget = g->second->retarget;
                o->clipsJer->capas = g->second->capas;
            }
            W3dJerCapasCambiaron(o);
        }
    }
    g_redraw = true;
}

bool SimHayScripts() {
    // una raiz de tipo JUEGO SIEMPRE tiene que simular (su fisica, sus particulas, sus hitbox corren
    // aunque todavia no tenga ningun script); una escena, solo si algo tiene script (el harness)
    if (W3dRaizActivaEsJuego()) return true;
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

// ARRANCA la partida en la raiz ACTIVA: cuerpos rigidos, escenas UI y los inicio() de sus scripts.
// Lo usan el Play y el cambio a otra escena 3D (cambiarEscena). gScripted ya tiene sus objetos.
static void ArrancarRaizActual() {
    // CUERPOS RIGIDOS: crear los de la escena ANTES de inicio() (lo que los
    // scripts importen en runtime lo agrega W3dImportarW3DAnexo por su cuenta)
    W3dRigidosLimpiar();
    W3dRigidosAsegurar(SceneCollection);
    W3dHitboxLimpiar();   // la partida arranca sin nadie "adentro" de ningun hitbox
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
    // SEGUNDA pasada de cuerpos rigidos: lo que los inicio() importaron con
    // importarW3D() entro ANTES de gActiva=true, asi que el hook del anexo
    // no lo dio de alta (Asegurar es idempotente: los ya creados no se tocan).
    W3dRigidosAsegurar(SceneCollection);
}

// cambiarEscena() a una ESCENA o un JUEGO 3D del proyecto, con la partida andando (lo aplica W3dEscena
// al final del frame). La raiz actual SUELTA su partida (scripts, rigidos, hitbox, animaciones:
// compartido() sobrevive, es lo que pasa datos entre niveles) y la pedida arranca DE CERO: si ya se
// jugo en esta partida vuelve a su base, igual que el juego compilado, que la recarga de su entrada.
// Un JUEGO arranca su simulacion; una ESCENA ademas reproduce su animacion desde el inicio (una
// cinematica: sus scripts pueden mirar animEscenaActual() y pedir la siguiente al terminar).
static void SimCambiarRaiz(int idx) {
    if (!gActiva) return;
    // el MODO de la partida (el timeline en "Juego") sigue en la raiz nueva aunque sea una escena:
    // lo que corre es el juego. El Stop vuelve a la raiz del Play y cada una recupera el suyo.
    const int kind = ActiveAnimKind; const bool esJuego = AnimEsJuego; const int ini = StartFrame;
    for (size_t i = 0; i < gScripted.size(); i++) W3dScriptDescargarDe(gScripted[i]);
    for (size_t i = 0; i < gScriptedNuevos.size(); i++) W3dScriptDescargarDe(gScriptedNuevos[i]);
    gScripted.clear();
    gScriptedNuevos.clear();
    W3dFisicaLimpiar();
    W3dRigidosLimpiar();
    W3dHitboxLimpiar();
    W3dAnimEscenaReset();
    W3dAnimObjetosReset();   // (los clips de objeto de la que se va dejan de sonar)
    BindsJuegoResetPunteros();
    W3dEscenaLimpiar();   // (las escenas UI de la que se va vuelven a verse, como las dejo el usuario)
    if (!W3dRaizCambiarActiva(idx)) {
        w3dLogfE("[juego] cambiarEscena: no pude abrir la escena %d", idx);
    } else {
        ActiveAnimKind = kind; AnimEsJuego = esJuego; StartFrame = ini;
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
        // la base de la escena destino: la del usuario (se toma la primera vez que se entra)
        Object* r = SceneCollection;
        bool ya = false;
        if (r == gRaizPlay) { Aplicar(gBaseEditor); ya = true; }
        for (size_t i = 0; i < gBasesRaices.size() && !ya; i++)
            if (gBasesRaices[i].raiz == r) { Aplicar(gBasesRaices[i].snap); ya = true; }
        if (!ya) {
            W3dStreamingPartidaPreparar();   // (sus diferidas, como al arrancar el Play: antes de la foto)
            gBasesRaices.push_back(SimBaseRaiz()); gBasesRaices.back().raiz = r; Snapshot(&gBasesRaices.back().snap);
        }
    }
    { extern void PropsOlvidarEscena(); PropsOlvidarEscena(); }
    Recorrer(SceneCollection, NULL, &gScripted);
    ArrancarRaizActual();
    W3dStreamingPartidaInicio();   // (las diferidas cerca de la escena nueva, como al arrancar el Play)
    W3dRaizArrancarCinematica();   // (una ESCENA: su animacion desde el inicio; un juego no hace nada)
    // el cache del rewind arranca de nuevo (lo grabado era de la otra escena): el snapshot que
    // TickReal toma al terminar este tick es el primero
    gGrab.clear();
    gGrabOffset = gTick + 1;
    w3dLogf("[juego] cambiarEscena: ahora corre '%s' (%d objeto(s) con script)",
            W3dRaizActiva() >= 0 ? W3dRaices()[(size_t)W3dRaizActiva()].nombre.c_str() : "?", (int)gScripted.size());
    g_redraw = true;
}

// lo que el Play hace en cada momento de una CINEMATICA (reproducirEscena, W3dRaices.h). La escena ya es
// la raiz activa en los tres: su foto antes de animarla, sus scripts al arrancar y, al salir, sus scripts
// afuera y la escena de vuelta como la dejo el usuario (el juego lo repone el swap de raiz: no se toco).
static void SimCine(int momento) {
    if (momento == W3D_CINE_ANTES) {
        Snapshot(&gCineBase);
        gCineScripted.clear();
        { extern void PropsOlvidarEscena(); PropsOlvidarEscena(); }
    } else if (momento == W3D_CINE_ENTRO) {
        Recorrer(SceneCollection, NULL, &gCineScripted);
        for (size_t i = 0; i < gCineScripted.size(); i++) {
            Object* o = gCineScripted[i];
            if (!W3dScriptCargar(o)) continue;
            SimReresolver(o);
            W3dScriptInicio(o);
        }
    } else {
        for (size_t i = 0; i < gCineScripted.size(); i++) W3dScriptDescargarDe(gCineScripted[i]);
        gCineScripted.clear();
        Aplicar(gCineBase);
        gCineBase = SimSnap();
        { extern void PropsOlvidarEscena(); PropsOlvidarEscena(); }
    }
}

// ---- instanciar(): lo nuevo arranca como si hubiera estado desde el Play (sus scripts cargan, resuelven sus
// refs e inician; sus cuerpos rigidos se crean; sus hitbox participan solos desde el proximo paso). Queda
// anotado para borrarlo al Stop
static void SimObjetoNuevo(Object* raiz) {
    // (tambien mientras se ARRANCA: un spawner hace instanciar() en su inicio())
    if ((!gActiva && !gArrancando) || !raiz) return;
    SimCreado c; c.o = raiz; c.serial = raiz->serial;
    gCreadosEnPlay.push_back(c);
    // los CUERPOS RIGIDOS antes que los inicio(), igual que al arrancar la partida (ArrancarRaizActual): un
    // fisicaVel()/fisicaImpulso() en el inicio() de lo instanciado encuentra su cuerpo (si no, se perdia)
    W3dRigidosAsegurar(raiz);
    std::vector<Object*> scripted;
    Recorrer(raiz, NULL, &scripted);
    for (size_t i = 0; i < scripted.size(); i++) {
        Object* o = scripted[i];
        if (!W3dScriptCargar(o)) continue;
        SimReresolver(o);
        W3dScriptInicio(o);
        gScriptedNuevos.push_back(o);
    }
    W3dRigidosAsegurar(raiz);   // (lo que esos inicio() importaron: Asegurar es idempotente)
    g_redraw = true;
}

// lo que las fotos del Play (el frame 0, el cache del rewind, la base de una cinematica y la de cada escena por
// la que paso la partida) tienen de objetos que se LIBERAN
// (y lo que las OTRAS entradas nombran de ellos: el RIEL de una camara que setRiel() puso en una curva que se
// libera. Sin esto, rebobinar -Aplicar- le devolvia a la camara una curva liberada)
static void PodarSnap(SimSnap& s, const std::set<Object*>& fuera) {
    for (size_t i = s.ents.size(); i-- > 0; ) if (fuera.count(s.ents[i].o)) s.ents.erase(s.ents.begin() + (long)i);
    for (size_t i = 0; i < s.ents.size(); i++) if (s.ents[i].riel && fuera.count((Object*)s.ents[i].riel)) s.ents[i].riel = NULL;
    for (size_t i = s.arms.size(); i-- > 0; ) if (fuera.count((Object*)s.arms[i].a)) s.arms.erase(s.arms.begin() + (long)i);
    for (size_t i = s.jer.size(); i-- > 0; ) if (fuera.count(s.jer[i].o)) s.jer.erase(s.jer.begin() + (long)i);
}
// TODAS las fotos de la partida
static void PodarFotos(const std::set<Object*>& fuera) {
    PodarSnap(gBase, fuera);
    PodarSnap(gBaseEditor, fuera);
    for (size_t i = 0; i < gGrab.size(); i++) PodarSnap(gGrab[i], fuera);
    PodarSnap(gCineBase, fuera);
    for (size_t i = 0; i < gBasesRaices.size(); i++) PodarSnap(gBasesRaices[i].snap, fuera);
}
static void QuitarDeLista(std::vector<Object*>& v, const std::set<Object*>& fuera) {
    for (size_t i = v.size(); i-- > 0; ) if (fuera.count(v[i])) v.erase(v.begin() + (long)i);
}
static void JuntarSub(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarSub(o->Childrens[i], out);
}
// 'o' (o algun ancestro suyo) nacio en este Play?
static bool CreadoEnPlay(Object* o) {
    for (Object* p = o; p; p = p->Parent)
        for (size_t i = 0; i < gCreadosEnPlay.size(); i++)
            if (gCreadosEnPlay[i].o == p && gCreadosEnPlay[i].serial == p->serial) return true;
    return false;
}
// ---- destruir() (al final del frame): sale de las listas del Play; lo creado jugando se LIBERA, lo del usuario
// se DESCUELGA (el Stop lo devuelve a su lugar)
static void SimObjetoDestruir(Object* o) {
    if (!o) return;
    std::vector<Object*> sub;
    JuntarSub(o, sub);
    std::set<Object*> fuera(sub.begin(), sub.end());
    QuitarDeLista(gScripted, fuera);
    QuitarDeLista(gScriptedNuevos, fuera);
    QuitarDeLista(gCineScripted, fuera);
    gLuaMsAcum.clear();   // (paralelo a gScripted: el proximo tick lo rearma)
    if (CreadoEnPlay(o)) {
        PodarFotos(fuera);
        for (size_t i = gCreadosEnPlay.size(); i-- > 0; )
            if (fuera.count(gCreadosEnPlay[i].o)) gCreadosEnPlay.erase(gCreadosEnPlay.begin() + (long)i);
        gPodando = true;   // (las listas y las fotos ya estan podadas: el gancho del destructor no repite)
        W3dScriptDestruirObjeto(o);
        gPodando = false;
        return;
    }
    // del USUARIO: afuera del arbol hasta el Stop (vivo: las fotos lo siguen nombrando sin peligro)
    for (size_t i = 0; i < sub.size(); i++) {
        W3dScriptDescargarDe(sub[i]);
        W3dRigidosOlvidar(sub[i]);
        W3dHitboxOlvidar(sub[i]);
        W3dFisicaOlvidar(sub[i]);
        W3dObjetoVivoMarcar(sub[i], false);   // (para lua ya no existe, como en el juego compilado)
    }
    // ...y nadie lo sigue teniendo en objeto("x"): objeto() da nil, igual que en el juego compilado (que lo
    // libera con W3dScriptDestruirObjeto). El Stop descarga todos los scripts: no hay nada que devolver
    W3dScriptSoltarRefsA(sub);
    for (size_t i = ObjSelects.size(); i-- > 0; ) if (fuera.count(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + (long)i);
    if (fuera.count(ObjActivo)) ObjActivo = NULL;
    SimDestruido d;
    d.o = o; d.serial = o->serial;
    d.padre = o->Parent; d.padreSerial = o->Parent ? o->Parent->serial : 0; d.indice = -1;
    if (o->Parent) {
        std::vector<Object*>& ch = o->Parent->Childrens;
        for (size_t i = 0; i < ch.size(); i++) if (ch[i] == o) { d.indice = (int)i; ch.erase(ch.begin() + (long)i); break; }
    }
    gDestruidosEnPlay.push_back(d);
    g_redraw = true;
}
// ---- el STREAMING (io/Streaming.h) descarga lo que genero una instancia: se LIBERA siempre, tambien lo del usuario
// (lo generado sale de la definicion: el streaming lo anota y el Stop lo vuelve a generar). Como destruir() de lo creado
// jugando: fuera de las listas y de las fotos, y W3dScriptDestruirObjeto
static void SimObjetoLiberar(Object* o) {
    if (!o) return;
    std::vector<Object*> sub;
    JuntarSub(o, sub);
    std::set<Object*> fuera(sub.begin(), sub.end());
    // lo que un script DESTRUYO adentro (descolgado, esperando el Stop para volver a su padre): su padre se va, asi que
    // se va con el (es lo generado de la misma instancia: el Stop la regenera entera)
    std::vector<Object*> descolgados;
    for (size_t k = gDestruidosEnPlay.size(); k-- > 0; )
        if (fuera.count(gDestruidosEnPlay[k].padre)) {
            descolgados.push_back(gDestruidosEnPlay[k].o);
            gDestruidosEnPlay.erase(gDestruidosEnPlay.begin() + (long)k);
        }
    for (size_t i = 0; i < descolgados.size(); i++) {
        std::vector<Object*> ds;
        JuntarSub(descolgados[i], ds);
        for (size_t j = 0; j < ds.size(); j++) { fuera.insert(ds[j]); W3dObjetoVivoMarcar(ds[j], true); }
    }
    QuitarDeLista(gScripted, fuera);
    QuitarDeLista(gScriptedNuevos, fuera);
    QuitarDeLista(gCineScripted, fuera);
    gLuaMsAcum.clear();   // (paralelo a gScripted: el proximo tick lo rearma)
    PodarFotos(fuera);
    for (size_t i = gCreadosEnPlay.size(); i-- > 0; )
        if (fuera.count(gCreadosEnPlay[i].o)) gCreadosEnPlay.erase(gCreadosEnPlay.begin() + (long)i);
    gPodando = true;   // (las listas y las fotos ya estan podadas: el gancho del destructor no repite)
    W3dScriptDestruirObjeto(o);
    for (size_t i = 0; i < descolgados.size(); i++) W3dLiberarSubarbol(descolgados[i]);   // (ya sin padre)
    gPodando = false;
}
// al Stop: lo DESTRUIDO del usuario vuelve a su lugar (al reves: los indices guardados valen) y lo CREADO jugando
// se libera
static void SimDeshacerCreadosYDestruidos() {
    for (size_t k = gDestruidosEnPlay.size(); k-- > 0; ) {
        SimDestruido& d = gDestruidosEnPlay[k];
        // (lo descolgado es NUESTRO hasta aca: nadie mas lo pudo liberar; vuelve a ser un objeto vivo)
        std::vector<Object*> sub;
        JuntarSub(d.o, sub);
        for (size_t i = 0; i < sub.size(); i++) W3dObjetoVivoMarcar(sub[i], true);
        if (!d.padre) continue;
        std::vector<Object*>& ch = d.padre->Childrens;
        const int idx = (d.indice < 0 || d.indice > (int)ch.size()) ? (int)ch.size() : d.indice;
        ch.insert(ch.begin() + idx, d.o);
        d.o->Parent = d.padre;
    }
    gDestruidosEnPlay.clear();
    gPodando = true;   // (la partida se termina: el gancho del destructor no poda nada)
    for (size_t k = gCreadosEnPlay.size(); k-- > 0; ) {
        SimCreado c = gCreadosEnPlay[k];
        if (W3dObjetoVivoSerial(c.o, c.serial)) W3dScriptDestruirObjeto(c.o);
    }
    gPodando = false;
    gCreadosEnPlay.clear();
    gScriptedNuevos.clear();
}

// ============================================================================
//  GUARDAR CON LA PARTIDA ANDANDO (io/GuardarW3D.cpp): el archivo lleva la escena del USUARIO. Mientras dura el
//  guardado, lo que un script destruyo vuelve a colgar de su padre (en su lugar) y lo que instanciar() creo sale
//  del arbol; SimGuardadoFin deja las dos cosas exactamente como estaban y la partida sigue (no corre ningun tick
//  en el medio). Sin partida no hacen nada.
// ============================================================================
struct SimSacadoGuardar { Object* o; Object* padre; int indice; };
static std::vector<SimSacadoGuardar> gSacadosGuardar;
static bool gGuardandoEnPlay = false;
static int IndiceEnHijos(Object* padre, Object* o) {
    for (size_t i = 0; padre && i < padre->Childrens.size(); i++) if (padre->Childrens[i] == o) return (int)i;
    return -1;
}
void SimGuardadoInicio() {
    if (!gActiva || gGuardandoEnPlay) return;
    gGuardandoEnPlay = true;
    // lo DESTRUIDO del usuario, a su lugar (al reves, como el Stop: los indices guardados valen) y VIVO otra vez
    // mientras tanto (el guardado sincroniza los overrides de una instancia comparando lo que genero: con sus
    // objetos "muertos" los perdia)
    for (size_t k = gDestruidosEnPlay.size(); k-- > 0; ) {
        SimDestruido& d = gDestruidosEnPlay[k];
        std::vector<Object*> sub;
        JuntarSub(d.o, sub);
        for (size_t i = 0; i < sub.size(); i++) W3dObjetoVivoMarcar(sub[i], true);
        if (!d.padre || IndiceEnHijos(d.padre, d.o) >= 0) continue;
        std::vector<Object*>& ch = d.padre->Childrens;
        const int idx = (d.indice < 0 || d.indice > (int)ch.size()) ? (int)ch.size() : d.indice;
        ch.insert(ch.begin() + idx, d.o);
        d.o->Parent = d.padre;
    }
    // lo CREADO jugando, afuera (de adelante hacia atras: se vuelve a colgar al reves)
    gSacadosGuardar.clear();
    for (size_t i = 0; i < gCreadosEnPlay.size(); i++) {
        const SimCreado& c = gCreadosEnPlay[i];
        if (!W3dObjetoVivoSerial(c.o, c.serial)) continue;
        Object* p = c.o->Parent;
        const int idx = IndiceEnHijos(p, c.o);
        if (idx < 0) continue;
        p->Childrens.erase(p->Childrens.begin() + idx);
        SimSacadoGuardar s; s.o = c.o; s.padre = p; s.indice = idx;
        gSacadosGuardar.push_back(s);
    }
}
void SimGuardadoFin() {
    if (!gGuardandoEnPlay) return;
    gGuardandoEnPlay = false;
    for (size_t k = gSacadosGuardar.size(); k-- > 0; ) {
        SimSacadoGuardar& s = gSacadosGuardar[k];
        std::vector<Object*>& ch = s.padre->Childrens;
        const int idx = (s.indice > (int)ch.size()) ? (int)ch.size() : s.indice;
        ch.insert(ch.begin() + idx, s.o);
    }
    gSacadosGuardar.clear();
    for (size_t k = 0; k < gDestruidosEnPlay.size(); k++) {
        SimDestruido& d = gDestruidosEnPlay[k];
        const int idx = IndiceEnHijos(d.padre, d.o);
        if (idx >= 0) d.padre->Childrens.erase(d.padre->Childrens.begin() + idx);
        std::vector<Object*> sub;
        JuntarSub(d.o, sub);
        for (size_t i = 0; i < sub.size(); i++) W3dObjetoVivoMarcar(sub[i], false);   // (para lua sigue sin existir)
    }
}

// ============================================================================
//  UN OBJETO SE LIBERA CON LA PARTIDA ANDANDO por un camino que no es destruir(): un paso del historial que se
//  cae con lo que tenia borrado, algo que el editor regenera o descarga... Las listas del Play (los objetos con
//  script, los nuevos, los de una cinematica) y sus fotos (el frame 0, el cache del rewind, la escena del
//  usuario, las bases de cada escena) no pueden quedar apuntandolo: el tick siguiente leia memoria liberada.
//  Es un piso de memoria (el editor igual bloquea lo que regenera con el Play andando). Lo engancha el
//  destructor de Object (W3dDesvincularRegistrar, Objects.h); sin partida no hace nada.
// ============================================================================
static void SimDesvincular(Object* borrado) {
    // (guardando: lo unico que se libera son las raices que el guardado cargo solo para escribirlas, que no estaban
    //  cargadas en la partida y no estan en ninguna lista ni foto; se saltea para no recorrer las fotos por cada una)
    if (!gActiva || gPodando || gGuardandoEnPlay || !borrado) return;
    std::set<Object*> fuera;
    fuera.insert(borrado);
    QuitarDeLista(gScripted, fuera);
    QuitarDeLista(gScriptedNuevos, fuera);
    QuitarDeLista(gCineScripted, fuera);
    gLuaMsAcum.clear();   // (paralelo a gScripted: el proximo tick lo rearma)
    PodarFotos(fuera);
    for (size_t i = gCreadosEnPlay.size(); i-- > 0; )
        if (gCreadosEnPlay[i].o == borrado) gCreadosEnPlay.erase(gCreadosEnPlay.begin() + (long)i);
    // lo del usuario que un script destruyo espera el Stop colgado de su padre: si el padre se fue, vuelve a la
    // raiz del Play (al final)
    for (size_t i = 0; i < gDestruidosEnPlay.size(); i++)
        if (gDestruidosEnPlay[i].padre == borrado) { gDestruidosEnPlay[i].padre = gRaizPlay; gDestruidosEnPlay[i].indice = -1; }
}
namespace {
struct SimEngancharDesvincular { SimEngancharDesvincular() { W3dDesvincularRegistrar(SimDesvincular); } } gSimEngancheDesvincular;
}

static void SimPlay() {
    // volcar a disco/overlay los .lua editados en el IDE (sucios) ANTES de leerlos: sino Play corre el .lua VIEJO
    // (el cambio vivia solo en el buffer del IDE). Asi cambiar un valor en el script y dar Play YA se ve.
    { extern void IDEGuardarSucios(); IDEGuardarSucios(); }
    // ...y la vista de un clip de jerarquia que se estaba editando vuelve a su clip: lo que animObjeto()
    // reproduce es la biblioteca (compartida), no la vista
    W3dClipsVistasSincronizar();
    gScripted.clear();
    Recorrer(SceneCollection, NULL, &gScripted);   // en el orden del arbol (outliner)
    // sin scripts no hay partida... salvo en un JUEGO, que simula igual (fisica, particulas, hitbox)
    if (gScripted.empty() && !W3dRaizActivaEsJuego()) return;
    // STREAMING (io/Streaming.h): las instancias DIFERIDAS sueltan lo que generaron (quedan como en el juego compilado
    // recien abierto: el streaming las carga jugando) ANTES de la foto y de juntar los scripts; el Stop las repone
    W3dStreamingPartidaPreparar();
    gScripted.clear();
    Recorrer(SceneCollection, NULL, &gScripted);
    // snapshot de la escena DEL USUARIO (antes de que inicio() toque nada): es lo
    // que Stop restaura. (Antes se pisaba con el snapshot post-inicio de abajo y
    // Stop dejaba pegado lo que los scripts movieron en inicio().)
    Snapshot(&gBaseEditor);
    gRaizPlay = SceneCollection;
    gBasesRaices.clear();
    W3dRaicesSetCambioJuego(SimCambiarRaiz);   // cambiarEscena() a otra escena 3D
    W3dRaicesSetCine(SimCine);                  // reproducirEscena(): una cinematica con el juego en pausa
    // instanciar() / destruir() de lua: lo nuevo arranca, lo destruido se va (y el Stop lo arregla)
    gCreadosEnPlay.clear(); gDestruidosEnPlay.clear(); gScriptedNuevos.clear();
    W3dScriptSetObjetoNuevo(SimObjetoNuevo);
    W3dScriptSetObjetoDestruir(SimObjetoDestruir);
    W3dScriptSetObjetoLiberar(SimObjetoLiberar);   // (lo que descarga el streaming)
    W3dAnimObjetosReset();                      // (ningun clip de objeto sonando de una partida anterior)
    // las BASES del mix de escenas (la transform de cada objeto antes de mezclarlo): Stop las
    // repone, asi un objeto que el mix toca por primera vez jugando no se queda con la base
    // de la partida para el Mix del editor
    W3dMixEscenasBasesGuardar();
    // (lo que un inicio() instancia ya es de la partida: sus scripts arrancan y el Stop lo borra)
    gArrancando = true;
    ArrancarRaizActual();
    // ...y las diferidas que estan CERCA se generan ya (bloqueante: el primer frame esta completo) y arrancan como lo
    // instanciado, despues de los inicio() de la escena: igual que en el juego compilado (W3dGameInicio)
    W3dStreamingPartidaInicio();
    // el Play de una raiz de tipo ESCENA (solo desde el harness o un proyecto armado a mano: la UI
    // simula los JUEGOS) arranca como en el juego compilado: con su animacion desde el inicio
    W3dRaizArrancarCinematica();
    gArrancando = false;
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
            // (y adentro de una instancia de prefab, primero lo de SU instancia: W3dEscenaBuscarRef)
            Object* obj = W3dEscenaBuscarRef(o, val);
            if (obj) W3dScriptResolverRef(o, (int)e, prop, obj);
            W3dScriptResolverOpcion(o, (int)e, prop, val);
            // el MISMO par sirve para las propiedades de VALOR (tipo 2): el que
            // corresponda lo decide el .lua al leer (objeto/opcion/propiedad)
            W3dScriptResolverValor(o, (int)e, prop, val);
        }
    }
}

// la ENTRADA del pad (y las flechas del teclado) hacia los scripts: la leen el tick de la partida y el de
// una cinematica (un "saltar" con un boton)
static void EntradaPad() {
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
    // los GATILLOS analogicos (LT / RT): pedales de un juego de autos (gatillo() en lua, 0..1)
    W3dScriptGatillo(0, axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]);
    W3dScriptGatillo(1, axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]);
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
}
// UN TICK DE CINEMATICA (reproducirEscena): el juego en PAUSA (ni su fisica ni sus scripts ni sus
// animaciones: su arbol ni esta en el activo) y corre la ESCENA: su animacion, sus esqueletos, sus clips
// de objeto y sus scripts. No avanza el tick de la partida ni graba en el cache: el rewind es del juego.
static void TickCine(float dt) {
    { extern void W3dCamarasRielTick(); W3dCamarasRielTick(); }
    EntradaPad();
    for (size_t i = 0; i < gCineScripted.size(); i++)
        if (gCineScripted[i]->visible) W3dScriptActualizar(gCineScripted[i], dt);
    W3dCineTick(dt);
    W3dAnimObjetosTick(dt);
    W3dArmaturesJuegoTick(dt);
    BindsJuegoSnapshotPunteros();
    W3dScriptFinFrame();
    // la vuelta al juego (termino / pararEscena3D) y su alTerminar(), al final del frame
    W3dEscenaAplicarPendiente();
    if (!PlayAnimation) UpdateAnimations(dt);
    g_redraw = true;
}

static void TickReal(float dt) {
    if (W3dCineActiva()) { TickCine(dt); return; }
    // FISICA del Core (velocidad + rebotes): PRIMERO se integran las posiciones y despues
    // corren los scripts, asi el script ve la posicion nueva y resuelve el choque (rebotar)
    // en el MISMO frame. Es el mismo orden y el mismo dt que el runtime compilado
    // (W3dGameActualizar en game/w3drun.cpp) -> el juego se comporta igual en los dos lados.
    W3dFisicaPaso(dt);
    // CUERPOS RIGIDOS (cajas con masa): mismo orden que la fisica minima, y los
    // scripts leen contactos() de ESTE paso en el mismo frame.
    W3dRigidosPaso(dt);
    // HITBOX: solapes de ESTE paso (despues de mover los cuerpos) -> alEntrar / alQuedarse /
    // alSalir, y los contactos nuevos de los rigidos -> alTocar. Antes de los actualizar():
    // el script ve el evento y su actualizar() del mismo tick ya puede reaccionar. Mismo lugar
    // que en el runtime compilado (W3dGameActualizar).
    W3dHitboxPaso(dt);
    // CAMARAS CON RIEL: el indice fraccionario (rielIndice, el 5to valor de rielDe)
    // lo horneaba SOLO el dibujo (Viewport3D::UpdateViewOrbit -> UpdatePosition), asi
    // que en ticks sin render -- harness `simplay`, o el primer tick antes del primer
    // frame -- los scripts leian el indice VIEJO y una VisZona modo curva recortaba
    // con la celda de OTRO nodo (cunas negras en el material de referencia). Se hornea
    // TAMBIEN aca, antes de correr los scripts: rielDe() queda fresco en el tick, con
    // el mismo valor que dejaria un frame dibujado entre tick y tick (UpdatePosition
    // es deterministica e idempotente: el dibujo posterior recalcula lo mismo).
    { extern void W3dCamarasRielTick(); W3dCamarasRielTick(); }
    EntradaPad();
    extern double W3dNowMs();
    double _lua0 = W3dNowMs();
    int _act = 0;
    if (gLuaMsAcum.size() != gScripted.size()) gLuaMsAcum.assign(gScripted.size(), 0.0);
    for (size_t i = 0; i < gScripted.size(); i++) {
        // solo corren los scripts de la escena ACTIVA (y visibles): un objeto invisible o
        // de una escena inactiva no ejecuta su logica. Sin multi-escena EsDeActiva cae a
        // la regla vieja (solo visible), asi que el 3D/una-sola-UI se comporta igual.
        if (!W3dEscenaEsDeActiva(gScripted[i])) continue;
        double _s0 = W3dNowMs();
        W3dScriptActualizar(gScripted[i], dt);
        gLuaMsAcum[i] += W3dNowMs() - _s0;   // a quien se le va el tiempo (SimLuaPerfTop)
        _act++;                              // objeto con script que EFECTIVAMENTE corrio (visible/escena activa)
    }
    g_luaScriptsActivos = _act;              // el "solo calcular las cosas cercanas": los no-visibles no cuentan
    g_luaScriptsTotal   = (int)gScripted.size();
    g_luaTickMs         = W3dNowMs() - _lua0;
    // ANIMACION DE ESCENA pedida por los scripts (animEscena de lua): avanzar el
    // reloj y APLICAR las curvas. Va DESPUES del bucle de scripts para que un
    // animEscena() recien pedido pose los objetos en ESTE mismo frame (el editor
    // solo evalua estas curvas con kind 0; en el juego el dueno es este tick).
    W3dAnimEscenaTick(dt);
    // CLIPS DE OBJETO (animObjeto de lua): cada objeto con su cabezal, varios a la vez. Mismo lugar
    // que la animacion de escena (en el runtime tambien).
    W3dAnimObjetosTick(dt);
    // ESQUELETOS: cada armature avanza su clip con su propio reloj (animClip de lua). Despues de los scripts,
    // igual que la animacion de escena: el clip recien pedido arranca en ESTE frame. (Mismo lugar en el runtime.)
    W3dArmaturesJuegoTick(dt);
    // SNAPSHOT del estado apretado para el flanco de apretado() del PROXIMO frame (DESPUES de los scripts,
    // igual que el runtime en W3dGameActualizar): asi apretado() funciona identico en el Play y compilado.
    BindsJuegoSnapshotPunteros();
    // STREAMING (io/Streaming.h): cargar lo que se acerco y descargar lo que se alejo, fuera de los actualizar() y ANTES
    // del fin de frame (lo que genera arranca su actualizar() desde el proximo tick). Mismo lugar que en el runtime
    W3dStreamingTick();
    W3dScriptFinFrame();   // idem para teclaApretada()/botonApretado(): UNA vez, despues de TODOS los objetos
                           // (y los destruir() del frame: ver SimObjetoDestruir)
    // lo que instanciar() creo en este tick entra a la lista (su inicio() ya corrio): su actualizar(), desde el proximo
    if (!gScriptedNuevos.empty()) { gScripted.insert(gScripted.end(), gScriptedNuevos.begin(), gScriptedNuevos.end()); gScriptedNuevos.clear(); }
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
    if (w3dEngine::W3dAudioPaused()) w3dEngine::W3dAudioPause(false);   // Play / reanudar: el mixer sigue donde estaba
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
    W3dCineAbortar();   // (el cache es del JUEGO: viajar en el tiempo deja una cinematica que estuviera sonando)
    if (tick < gGrabOffset) tick = gGrabOffset;
    if (tick > gGrabOffset + (int)gGrab.size() - 1) tick = gGrabOffset + (int)gGrab.size() - 1;
    gTick = tick;
    Aplicar(gGrab[gTick - gGrabOffset]);
    W3dHitboxLimpiar();   // el viaje en el tiempo no restaura quien estaba adentro: se recalcula
    if (AnimEsJuego || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
}

bool SimStep(int dir) {
    if (!gActiva) return false;
    if (gGrab.empty()) return true;   // sin cache (destildado o juego compilado): no hay rewind -> no-op
                                      // (evita gGrab[gTick] fuera de rango: gTick avanza pero gGrab no crece)
    if (dir < 0) {
        if (gTick <= gGrabOffset) return true;   // el cache rodo: mas atras no hay
        W3dCineAbortar();   // (el rewind es del juego: una cinematica que sonaba se deja)
        gTick--;
        Aplicar(gGrab[gTick - gGrabOffset]);     // VIAJE EN EL TIEMPO: re-ver el anterior
        W3dHitboxLimpiar();                      // (los pares de hitbox se recalculan al seguir)
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
    // el "Parar" del editor: nada sigue sonando (la musica secuenciada, los loops del motor...)
    w3dEngine::W3dAudioPause(false);
    w3dEngine::W3dSoundStopAll();
    if (!gActiva) return;
    // una CINEMATICA sonando: la escena vuelve a como estaba y el juego a ser la raiz activa (sin su
    // alTerminar: la partida se corta)
    W3dCineAbortar();
    W3dRaicesSetCine(NULL);
    W3dRaicesSetCambioJuego(NULL);
    // las OTRAS escenas 3D por las que paso la partida vuelven a como las dejo el usuario (cada una
    // con SU contexto: el snapshot tambien repone sus capas del Mix) y se vuelve a la del Play
    if (!gBasesRaices.empty() || (gRaizPlay && SceneCollection != gRaizPlay)) {
        W3dEscenaLimpiar();
        for (size_t i = 0; i < gBasesRaices.size(); i++) {
            const int k = W3dRaizDeObjeto(gBasesRaices[i].raiz);
            if (k >= 0 && W3dRaizUsar(k)) Aplicar(gBasesRaices[i].snap);
        }
        const int k = W3dRaizDeObjeto(gRaizPlay);
        if (k >= 0) W3dRaizCambiarActiva(k);
        { extern void PropsOlvidarEscena(); PropsOlvidarEscena(); }
    }
    gBasesRaices.clear();
    gRaizPlay = NULL;
    // lo que los scripts crearon jugando se va y lo que destruyeron vuelve (antes de reponer la foto: la foto
    // nombra a lo destruido, que tiene que estar de vuelta en su lugar)
    W3dScriptSetObjetoNuevo(NULL);
    W3dScriptSetObjetoDestruir(NULL);
    W3dScriptSetObjetoLiberar(NULL);
    SimDeshacerCreadosYDestruidos();
    Aplicar(gBaseEditor);   // la escena del usuario, NO el frame 0 post-inicio()
    // STREAMING: lo que cargo jugando ya se libero (era creado en el Play); las diferidas que el Play solto se vuelven a
    // generar como estaban y se sueltan los pedidos de la partida
    W3dStreamingPartidaFin();
    W3dMixEscenasBasesReponer();   // las bases del mix, como antes del Play (no mueve nada)
    W3dScriptDescargarTodo();
    W3dScriptSoltarTeclas();
    W3dAnimEscenaReset();   // que no quede una animEscena() sonando para la proxima partida
    W3dAnimObjetosReset();  // ...ni un clip de objeto (animObjeto)
    W3dRigidosLimpiar();    // los cuerpos rigidos runtime mueren con la partida
    W3dHitboxLimpiar();     // y los pares de hitbox / contactos (sin eventos)
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
    else if (sdlk == SDLK_LSHIFT || sdlk == SDLK_RSHIFT) n = "shift";   // correr, agacharse...
    else if (sdlk == SDLK_LCTRL  || sdlk == SDLK_RCTRL)  n = "ctrl";
    else if (sdlk == SDLK_RETURN || sdlk == SDLK_KP_ENTER) n = "enter";
    else if (sdlk >= SDLK_0 && sdlk <= SDLK_9) { letra[0] = (char)('0' + (sdlk - SDLK_0)); n = letra; }
    if (n) W3dScriptTecla(n, down);
#endif
}
