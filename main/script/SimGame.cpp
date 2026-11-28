// ============================================================================
//  SimGame.cpp — ver SimGame.h.
// ============================================================================
#include "animation/SkeletalAnimation.h" // W3dArmaturesGameTick
#include "script/SimGame.h"
#include "undo/Undo.h"                // el deshacer de lo que hace un script FUERA del juego (botones)
#include "base/W3dInteractionState.h" // (idem: crear/borrar lo que pidio el script, solo en el Modo Objeto)
#include "script/W3dScript.h"
#include "script/BindsGame.h"        // alimentar el estado de los binds de juego (pressed/etc) en el Play
#include "physics/W3dPhysics.h"      // paso de fisica del Core (el MISMO que corre el juego compilado)
#include "physics/W3dRigid.h"        // cuerpos rigidos del Core (cajas + impulsos + contactos)
#include "physics/W3dHitbox.h"       // hitbox del Core (alEntrar/alQuedarse/alSalir + alTocar)
#include "W3dScene.h"                // motor multi-escena COMPARTIDO (activa / init perezoso / cambio pendiente)
#include "W3dRoots.h"                // changeScene() a otra ESCENA 3D del proyecto (el Stop vuelve a la de antes)
#include "io/Streaming.h"            // las instancias DIFERIDAS cargan/descargan jugando (el Stop las repone)
#include "io/EditorRoots.h"          // W3dActivateRoot: el Play arranca con la PRIMERA escena abierta
#include "ViewPorts/Notifications.h" // (si no se puede pasar a esa escena, el aviso)
#include "config/W3dLang.h"
#include "w3dlog.h"
#include "audio/W3dAudio.h"           // Parar = cortar TODAS las voces; Play = despausar el mixer
#include "objects/Objects.h"
#include "objects/Armature.h"         // el estado de JUEGO de los esqueletos (clip, cabezal, capas, transicion)
#include "animation/W3dAnimSet.h"      // las CAPAS de clips de jerarquia de una raiz (objectLayer las cambia jugando)
#include "objects/Light.h"            // el snapshot guarda/restaura el color difuso (setColor/setEnergy)
#include "objects/Particles.h"        // idem el activo del emisor (setEmitting)
#include "objects/Camera.h"           // idem el RIEL de la camara (setRail/setRailNode son estado de juego)
#include "WhiskUI/objects/UI.h"
#include "WhiskUI/objects/Element2D.h"
#include "WhiskUI/objects/Text2D.h"
#include "WhiskUI/render/UIOverlay.h"
#include "render/RenderOptions.h"      // g_redraw
#include "animation/Animation.h"       // CurrentFrame / StartFrame / EndFrame / PlayAnimation
#include "objects/Gamepad.h"           // axisState / buttonState (el pad SDL del editor)
#include "objects/Mesh.h"              // Mesh::animations (la vertex anim tambien es estado de partida)
#include "animation/VertexAnimation.h" // FindTargetAnim / EvalVertexAnim / UpdateAnimations
#include "ViewPorts/Editor2D.h"        // SimTouchScreen: mapeo screen->canvas real
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
    float rot2d, width, height, scrollX, scrollY, opacity;
    bool  es2d, isText, isLight;
    float dif[4];          // solo si isLight: el color difuso (lo tocan setColor/setEnergy)
    float amb[4], spe[4];  // ...y el ambient/specular (los animan las animaciones de escena y los clips de objeto)
    bool  isPart, partActive;  // emisor de particulas: setEmitting() lo apaga en juego
    bool  isFog; float fog[6];  // setFog(): densidad, color, inicio, fin
    // FISICA: la velocidad tambien es estado del juego. Sin guardarla, restaurar una foto reponia las
    // posiciones pero la pelota seguia con la velocidad de AHORA. Solo se guarda si el objeto tiene
    // cuerpo (casi ninguno lo tiene).
    bool  hasVel;
    float vx, vy, vz;
    // EL RIEL de una Camera: setRail/setRailNode lo mueven EN JUEGO (un cambio de sala salta
    // entre tres rieles), asi que es estado de partida igual que la velocidad. Sin esto, Stop
    // dejaba la camara del editor pegada al riel del bonus y el viaje en el tiempo reponia las
    // posiciones pero no el riel con el que se habian calculado.
    bool  isCam;
    Curve* rail;
    int   offRail;
    float railNode;
    bool  lookRail;   // modo de mirada: setRailLook() lo cambia EN JUEGO
    // VERTEX ANIMATION: el frame que muestra el personaje (y cada enemigo) TAMBIEN es
    // estado de partida. Reporte del dueno, textual: "el timeline la captura de
    // estados del whisk3d editor no tiene en cuenta la animacion del vertex
    // animation del personaje y los enemigos". Sin esto, el scrub / el paso atras reponian
    // las POSICIONES y dejaban la malla en la pose del ULTIMO frame simulado:
    // el personaje retrocedia por el nivel congelado en media zancada.
    // Se guardan los tres campos del reproductor (VertexAnimationActive):
    //   vaPlay = playFrame, la cabeza lectora en cuadros (float, interpolable);
    //   vaAnim = currentAnim, que clip esta sonando;
    //   vaNext = nextAnim, el clip que pidio animate() de lua y todavia no entro.
    // Restaurar los tres no alcanza: la POSE vive en mesh->vertex[]/normals[]/uv[],
    // asi que Aplicar ademas re-evalua (EvalVertexAnim) y reescribe la geometria.
    bool  hasVA;
    float vaPlay;
    int   vaAnim, vaNext;
    // HITBOX: setHitboxActive/setHitboxSize de lua lo cambian EN JUEGO (una puerta que se
    // apaga despues de abrirse): Stop tiene que devolverlo como lo dejo el usuario
    bool  isHb, hbActive;
    float hbSize[3];
    std::string text;
};
// ESQUELETOS: el estado de JUEGO de cada armature. animClip / animFrame / animSpeed /
// animTransition / animLayer de lua lo cambian EN JUEGO, y el clip activo y las capas del mix
// SE GUARDAN en el .w3d: sin esto, Stop dejaba el armature con el clip que habia elegido el
// script (y el siguiente guardado lo escribia en el archivo), y el viaje en el tiempo reponia
// las posiciones pero no el cabezal con el que se habian posado los huesos.
// Va APARTE de SimEnt (un vector por snapshot, solo con los armatures): casi ningun objeto es
// un esqueleto.
struct SimArm {
    Armature* a;
    int   clip;                         // activeAnim
    float frame, vel;                   // gameFrame, gameVel
    bool  loop, ended;                  // gameLoop, gameEnded
    int   retarget;                     // animRetarget de lua (se guarda en el .w3d)
    std::vector<W3dLayerAnim> layers;   // las capas del mix (animLayer las crea y las cambia jugando)
    // la TRANSICION en curso (animTransition): el tiempo que falta y la pose congelada desde la
    // que se funde. La pose solo se copia si hay una transicion andando (no pesa por tick).
    float transRest, transTotal;
    std::vector<Vector3> transT, transS;
    std::vector<Quaternion> transQ;
};
// lo de los CLIPS DE JERARQUIA de una raiz que el juego cambia y que va al .w3d: su RETARGET propio
// (animRetarget de lua) y sus CAPAS (objectLayer...). El Stop los devuelve como los dejo el usuario y el viaje en
// el tiempo repone los cabezales de las capas. Solo las raices que tienen algo de eso (casi ningun objeto).
struct SimHier {
    Object* o;
    int retarget;
    std::vector<W3dLayerAnim> layers;
};
struct SimSnap {
    std::vector<SimEnt> ents;
    std::vector<SimArm> arms;
    std::vector<SimHier> hier;
    // las capas del MIX DE ESCENAS (sceneLayer de lua): son globales y tambien van al .w3d
    std::vector<W3dLayerAnim> mixScenes;
};

static bool gActive = false;
// true mientras SimPlay ARRANCA la partida (los start() de StartCurrentRoot corren antes de gActive = true): lo
// que un start() crea con instantiate() -un spawner- es de la partida igual (SimNewObject)
static bool gStarting = false;
static SimSnap gBaseEditor;  // la escena DEL USUARIO antes de tocar nada (la restaura Stop)
// ESCENAS 3D (W3dRoots.h): changeScene() a otra escena del proyecto cambia la RAIZ activa sin
// destruir nada. La del Play (gRootPlay) vuelve con Stop; cada otra por la que paso la partida
// guarda SU base (como la dejo el usuario) para que el Stop la restaure y para que volver a ella
// en la misma partida arranque de cero, como en el juego compilado (que la recarga de su entrada).
struct SimBaseRoot { Object* root; SimSnap snap; };
static Object* gRootPlay = NULL;
static std::vector<SimBaseRoot> gBasesRoots;
// las raices que ya estaban cargadas al dar Play (las abrio el usuario: pueden tener cambios sin guardar). Las que
// cargo la PARTIDA con changeScene() se descargan cuando la partida se va de ellas (SimChangeRoot)
static std::set<Object*> gUserRoots;
// CINEMATICA (playScene, W3dRoots.h): la ESCENA que se reproduce como la dejo el usuario (se le
// devuelve al volver) y sus objetos con script (corren mientras dura; los del juego quedan en pausa)
static SimSnap gCinematicBase;
static std::vector<Object*> gCinematicScripted;
static int gTick = 0;
static std::vector<Object*> gScripted;
// ---- LO QUE UN SCRIPT CREA Y DESTRUYE JUGANDO (instanciar / destruir de lua, io/Prefabs.h) ----
// CREADOS: las raices que nacieron en el Play (por serial: la direccion se recicla). Se BORRAN al Stop: la
// escena del usuario vuelve a ser la de antes del Play.
// DESTRUIDOS: lo del USUARIO que un script destruyo. No se libera: se DESCUELGA (sus scripts, su cuerpo y sus
// pares se van) y el Stop lo vuelve a colgar donde estaba. Lo creado en el Play si se libera de verdad.
// NUEVOS: los objetos con script que nacieron en este tick; entran a gScripted al final (no en medio del bucle
// de actualizar, que recorre gScripted por indice).
struct SimCreated { Object* o; unsigned serial; };
struct SimDestroyed { Object* o; unsigned serial; Object* parent; unsigned parentSerial; int index; };
static std::vector<SimCreated> gCreatedInPlay;
static std::vector<SimDestroyed> gDestroyedInPlay;
static std::vector<Object*> gScriptedNew;
// true mientras SimJuego libera algo que YA saco de sus listas y fotos (destroy() de lo creado jugando, el Stop):
// el gancho del destructor (SimUnlink) no tiene nada que hacer
static bool gPruning = false;
// INSTRUMENTACION de scripts (para el [PERF] y el overlay de stats): cuantos corrieron este tick, cuantos hay,
// y ms del loop. Los conteos son ints puros (sirven en PC y Symbian); el ms usa W3dNowMs (real en las dos:
// SDL de alta resolucion en PC, NTickCount ~1ms en Symbian).
int    g_luaActiveScripts = 0;
int    g_luaScriptsTotal  = 0;
double g_luaTickMs        = 0.0;
// ms ACUMULADOS por objeto-con-script (paralelo a gScripted): dice CUAL script
// se come el tiempo, no solo el total. SimLuaPerfTop lo vuelca y lo resetea.
static std::vector<double> gLuaMsAccum;

// Vuelca los 3 scripts que MAS ms acumularon desde el ultimo volcado, como
// "Juego:41 agua:12 camara:2" (promedio POR TICK si frames > 1), y resetea el
// acumulador. Lo consume el [PERF] de Symbian; en PC sirve igual desde el log.
void SimLuaPerfTop(char* buf, int bufLen, int frames) {
    if (!buf || bufLen < 24) return;
    buf[0] = 0;
    if (frames < 1) frames = 1;
    char* p = buf; char* end = buf + bufLen - 24;   // margen para "nombre:9999 "
    for (int k = 0; k < 3 && p < end; k++) {
        size_t best = gLuaMsAccum.size(); double mms = 0.0;
        for (size_t i = 0; i < gLuaMsAccum.size() && i < gScripted.size(); i++)
            if (gLuaMsAccum[i] > mms) { mms = gLuaMsAccum[i]; best = i; }
        if (best >= gLuaMsAccum.size() || mms <= 0.0) break;
        char name[13]; size_t nl = gScripted[best]->name.size(); if (nl > 12) nl = 12;
        memcpy(name, gScripted[best]->name.c_str(), nl); name[nl] = 0;
        p += sprintf(p, "%s%s:%d", k ? " " : "", name, (int)(mms / frames + 0.5));
        gLuaMsAccum[best] = 0.0;
    }
    for (size_t i = 0; i < gLuaMsAccum.size(); i++) gLuaMsAccum[i] = 0.0;
}
// FLECHAS DEL TECLADO apretadas AHORA [arriba, abajo, izquierda, derecha]. Las
// mantiene SimKeySDL y las mezcla TickReal en el stick izquierdo, igual que el
// d-pad. Se sueltan al parar la partida (SimStop) para que una flecha que quedo
// apretada al cortar no arrastre al jugador en la partida siguiente.
static bool gArrow[4] = { false, false, false, false };

bool SimActive() { return gActive; }
// modo juego DE VERDAD (ver SimGame.h): timeline en "Juego" + partida cargada.
// Gate compartido de los overlays de edicion / chrome que el juego no muestra.
bool W3dGameRunning() { extern bool AnimIsGame; return AnimIsGame && gActive; }
int  SimCurrentFrame() { return gTick; }

static void Traverse(Object* o, SimSnap* snap, std::vector<Object*>* scripted) {
    if (!o) return;
    if (snap) {
        SimEnt e;
        e.o = o;
        e.px = o->pos.x; e.py = o->pos.y; e.pz = o->pos.z;
        e.rot = o->Rot(); e.rotEuler = o->rotEuler; e.rotAngle = o->rotAngle; e.rotAxis = o->rotAxis;
        e.scale = o->scale;
        e.vis = o->visible; e.renderiz = o->renderable;
        e.isLight = (o->getType() == ObjectType::light);
        if (e.isLight) for (int k = 0; k < 4; k++) { e.dif[k] = ((Light*)o)->diffuse[k];
                                                    e.amb[k] = ((Light*)o)->ambient[k]; e.spe[k] = ((Light*)o)->specular[k]; }
        else         for (int k = 0; k < 4; k++) e.dif[k] = e.amb[k] = e.spe[k] = 0.0f;
        e.isPart = (o->getType() == ObjectType::particles);
        e.partActive = e.isPart ? ((Particles*)o)->active : true;
        e.isFog = W3dFogHook && W3dFogHook(o, false, e.fog, 6);
        e.isCam = (o->getType() == ObjectType::camera);
        e.rail = NULL; e.offRail = 0; e.railNode = -1.0f; e.lookRail = false;
        if (e.isCam) {
            Camera* c = (Camera*)o;
            e.rail = c->Rail; e.offRail = c->offsetRail; e.railNode = c->railNode;
            e.lookRail = c->useRailRotation;
        }
        e.vx = e.vy = e.vz = 0.0f;
        e.hasVel = W3dPhysicsHas(o);
        if (e.hasVel) W3dPhysicsGetVel(o, &e.vx, &e.vy, &e.vz);
        e.es2d = UI2D_IsElement2D(o);
        e.isText = (o->getType() == ObjectType::text2d);
        e.rot2d = 0; e.width = 0; e.height = 0; e.scrollX = 0; e.scrollY = 0; e.opacity = 1;
        if (e.es2d) {
            Element2D* el = (Element2D*)o;
            e.rot2d = el->rot2d; e.width = el->width; e.height = el->height;
            e.scrollX = el->scrollX; e.scrollY = el->scrollY; e.opacity = el->opacity;
        } else if (o->getType() == ObjectType::ui) {
            e.width = ((UI*)o)->width; e.height = ((UI*)o)->height;
        }
        if (e.isText) e.text = ((Text2D*)o)->text;
        e.isHb = (o->getType() == ObjectType::hitbox);
        e.hbActive = true; e.hbSize[0] = e.hbSize[1] = e.hbSize[2] = 0.0f;
        if (e.isHb) {
            const W3dHitboxBase* hb = (const W3dHitboxBase*)o;
            e.hbActive = hb->active;
            for (int k = 0; k < 3; k++) e.hbSize[k] = hb->size[k];
        }
        // VERTEX ANIMATION: el reproductor de la malla (ver el comentario de SimEnt)
        e.hasVA = false; e.vaPlay = 0.0f; e.vaAnim = -1; e.vaNext = -1;
        if (o->getType() == ObjectType::mesh) {
            VertexAnimationActive* va = FindTargetAnim((Mesh*)o);
            if (va) { e.hasVA = true; e.vaPlay = va->playFrame;
                      e.vaAnim = va->currentAnim; e.vaNext = va->nextAnim; }
        }
        snap->ents.push_back(e);
        // ESQUELETOS: su estado de juego va aparte (ver SimArm)
        if (o->getType() == ObjectType::armature) {
            Armature* a = (Armature*)o;
            snap->arms.push_back(SimArm());
            SimArm& sa = snap->arms.back();
            sa.a = a;
            sa.clip = a->activeAnim;
            sa.frame = a->gameFrame; sa.vel = a->gameVel;
            sa.loop = a->gameLoop; sa.ended = a->gameEnded;
            sa.retarget = a->retarget;
            sa.layers = a->layers;
            sa.transRest = a->remainingTrans; sa.transTotal = a->transTotal;
            if (a->remainingTrans > 0.0f) { sa.transT = a->transT; sa.transS = a->transS; sa.transQ = a->transQ; }
        }
        // RETARGET y CAPAS de clips de jerarquia (ver SimHier)
        if (o->clipsHier && (o->clipsHier->retarget >= 0 || !o->clipsHier->layers.empty())) {
            snap->hier.push_back(SimHier());
            snap->hier.back().o = o;
            snap->hier.back().retarget = o->clipsHier->retarget;
            snap->hier.back().layers = o->clipsHier->layers;
        }
    }
    if (scripted && o->scriptData && !o->scriptData->scripts.empty())
        scripted->push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        Traverse(o->Childrens[i], snap, scripted);
}

// LOS SCRIPTS de cada objeto: lo del inspector (los valores de sus propiedades, las opciones, las refs, el ojo y los
// scripts que se agregan o se quitan). Lo que se cambia JUGANDO tambien es de la partida, y Stop lo devuelve como lo dejo
// el usuario: TODO lo que pasa jugando se pierde (como los objetos creados o destruidos y lo que esta en la foto).
// Va aparte de SimSnap: el viaje en el tiempo no lo necesita, y asi no se copian textos en cada cuadro.
struct SimScriptsObj { Object* o; std::vector<W3dScriptEntry> scripts; bool had; };
static std::vector<SimScriptsObj> gScriptsBase;
static void ScriptsSnapshot(Object* o) {
    if (!o) return;
    SimScriptsObj s; s.o = o; s.had = (o->scriptData != NULL);
    if (o->scriptData) s.scripts = o->scriptData->scripts;
    gScriptsBase.push_back(s);
    for (size_t i = 0; i < o->Childrens.size(); i++) ScriptsSnapshot(o->Childrens[i]);
}
static void LiveScripts(Object* o, std::set<Object*>& live) {
    if (!o) return;
    live.insert(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) LiveScripts(o->Childrens[i], live);
}
static void ScriptsRestore() {
    std::set<Object*> live;   // (solo los que siguen en la escena: lo creado jugando ya se fue)
    LiveScripts(SceneCollection, live);
    for (size_t i = 0; i < W3dRoots().size(); i++) if (W3dRoots()[i].root) LiveScripts(W3dRoots()[i].root, live);
    for (size_t i = 0; i < gScriptsBase.size(); i++) {
        SimScriptsObj& s = gScriptsBase[i];
        if (!live.count(s.o)) continue;
        if (s.had) {
            if (!s.o->scriptData) s.o->scriptData = new W3dScriptData();
            s.o->scriptData->scripts = s.scripts;
            if (s.o->scriptData->active >= (int)s.scripts.size()) s.o->scriptData->active = (int)s.scripts.size() - 1;
            if (s.o->scriptData->active < 0) s.o->scriptData->active = 0;
        } else if (s.o->scriptData) {
            s.o->scriptData->scripts.clear();   // (le agregaron scripts jugando)
        }
    }
    gScriptsBase.clear();
    extern void PropsScriptsRefresh(); PropsScriptsRefresh();   // las tarjetas de Propiedades, con lo repuesto
}

static void Snapshot(SimSnap* s) {
    s->ents.clear();
    s->arms.clear();
    s->hier.clear();
    Traverse(SceneCollection, s, NULL);
    s->mixScenes = g_mixScenes;
}
static void Apply(const SimSnap& s) {
    for (size_t i = 0; i < s.ents.size(); i++) {
        const SimEnt& e = s.ents[i];
        Object* o = e.o; if (!o) continue;
        o->pos.x = e.px; o->pos.y = e.py; o->pos.z = e.pz;
        // rotacion: se restauran el quaternion Y los displays TAL CUAL estaban (la puerta de SNAPSHOT
        // no re-deriva el euler del quaternion: eso se comeria las vueltas).
        o->SetRotSnapshot(e.rot, e.rotEuler);
        o->scale = e.scale;
        o->visible = e.vis; o->renderable = e.renderiz;
        if (e.isLight) for (int k = 0; k < 4; k++) { ((Light*)o)->diffuse[k] = e.dif[k];
                                                    ((Light*)o)->ambient[k] = e.amb[k]; ((Light*)o)->specular[k] = e.spe[k]; }
        if (e.isPart) ((Particles*)o)->active = e.partActive;
        if (e.isFog) { float v[6]; for (int k = 0; k < 6; k++) v[k] = e.fog[k]; W3dFogHook(o, true, v, 6); }
        if (e.isCam) {
            // el par (Riel, RailName) se repone junto: RailName es lo que guarda el .w3d
            Camera* c = (Camera*)o;
            c->Rail = e.rail;
            c->RailName = e.rail ? e.rail->name : std::string();
            c->offsetRail = e.offRail;
            c->railNode = e.railNode;
            c->useRailRotation = e.lookRail;
        }
        // la velocidad vuelve a la que tenia en ese frame (solo si ya tenia cuerpo: no se
        // le inventa uno a un objeto que en ese momento no se movia)
        if (e.hasVel) W3dPhysicsSetVel(o, e.vx, e.vy, e.vz);
        if (e.es2d) {
            Element2D* el = (Element2D*)o;
            el->rot2d = e.rot2d; el->width = e.width; el->height = e.height;
            el->scrollX = e.scrollX; el->scrollY = e.scrollY; el->opacity = e.opacity;
        } else if (o->getType() == ObjectType::ui) {
            ((UI*)o)->width = e.width; ((UI*)o)->height = e.height;
        }
        if (e.isText) ((Text2D*)o)->text = e.text;
        if (e.isHb) {
            W3dHitboxBase* hb = (W3dHitboxBase*)o;
            hb->active = e.hbActive;
            for (int k = 0; k < 3; k++) hb->size[k] = e.hbSize[k];
        }
        // VERTEX ANIMATION: reponer la cabeza lectora Y VOLVER A POSAR la malla.
        // Reponer playFrame solo no se ve: la pose vive en mesh->vertex[], que la
        // escribio EvalVertexAnim en el frame que se esta descartando. Por eso se
        // re-evalua aca -- es lo que hace que el scrub del timeline muestre el
        // frame de animacion que corresponde y no la ultima pose simulada.
        if (e.hasVA) {
            Mesh* m = (Mesh*)o;
            VertexAnimationActive* va = FindTargetAnim(m);
            if (va) {
                va->playFrame   = e.vaPlay;
                va->currentAnim = e.vaAnim;
                va->nextAnim    = e.vaNext;
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
        a->activeAnim = e.clip;
        a->gameFrame  = e.frame; a->gameVel = e.vel;
        a->gameLoop = e.loop; a->gameEnded = e.ended;
        if (a->retarget != e.retarget) W3dArmatureRetarget(a, e.retarget);
        a->layers = e.layers;
        a->remainingTrans = e.transRest; a->transTotal = e.transTotal;
        a->transT = e.transT; a->transS = e.transS; a->transQ = e.transQ;
        // y la POSE se recalcula: el cache de EvaluatePoseSkeleton mira el frame y el clip,
        // no las capas ni la transicion que se acaban de reponer
        a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->lastPoseFrameF = -999999.0f;
    }
    g_mixScenes = s.mixScenes;
    // RETARGET y CAPAS DE CLIPS DE JERARQUIA: los de ESE momento; una raiz que entonces no tenia nada de eso
    // (animRetarget / objectLayer se lo pusieron despues) vuelve a "el de cada clip" y sin capas
    {
        std::map<Object*, const SimHier*> saved;
        for (size_t k = 0; k < s.hier.size(); k++) if (s.hier[k].o) saved[s.hier[k].o] = &s.hier[k];
        for (size_t i = 0; i < s.ents.size(); i++) {
            Object* o = s.ents[i].o;
            if (!o) continue;
            std::map<Object*, const SimHier*>::iterator g = saved.find(o);
            if (g == saved.end()) {
                if (!o->clipsHier || (o->clipsHier->retarget < 0 && o->clipsHier->layers.empty())) continue;
                o->clipsHier->retarget = -1;
                o->clipsHier->layers.clear();
            } else {
                if (!o->clipsHier) o->clipsHier = new W3dHierRoot();
                o->clipsHier->retarget = g->second->retarget;
                o->clipsHier->layers = g->second->layers;
            }
            W3dHierLayersChanged(o);
        }
    }
    g_redraw = true;
}

bool SimHasScripts() {
    // una raiz de tipo JUEGO SIEMPRE tiene que simular (su fisica, sus particulas, sus hitbox corren
    // aunque todavia no tenga ningun script); una escena, solo si algo tiene script (el harness)
    if (W3dActiveRootIsGame()) return true;
    std::vector<Object*> s;
    Traverse(SceneCollection, NULL, &s);
    return !s.empty();
}

// buscar un objeto por NOMBRE en el arbol (para resolver las referencias)
static Object* FindByName(Object* o, const std::string& n) {
    if (!o) return NULL;
    if (o->name == n) return o;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* r = FindByName(o->Childrens[i], n);
        if (r) return r;
    }
    return NULL;
}

// init de UN subarbol: carga+resuelve+start() de cada objeto con script (en orden de
// arbol). reiniciar=true descarga antes (recrea los lua_State desde cero).
static void EditorInitSubtree(Object* o, bool restart) {
    if (!o) return;
    if (o->scriptData && !o->scriptData->scripts.empty()) {
        if (restart) W3dScriptUnloadFrom(o);
        if (W3dScriptLoad(o)) {
            SimReresolve(o);      // referencias (por nombre) y opciones, de TODOS sus scripts
            W3dScriptStart(o);
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        EditorInitSubtree(o->Childrens[i], restart);
}
// callback de init de UNA escena (lo registra W3dSceneSetInit): el motor multi-escena
// lo llama al arrancar la inicial y al hacer el init perezoso de cada escena destino.
static void EditorInitScene(UI* u, bool restart) { EditorInitSubtree((Object*)u, restart); }

// ARRANCA la partida en la raiz ACTIVA: cuerpos rigidos, escenas UI y los start() de sus scripts.
// Lo usan el Play y el cambio a otra escena 3D (changeScene). gScripted ya tiene sus objetos.
static void StartCurrentRoot() {
    // CUERPOS RIGIDOS: crear los de la escena ANTES de start() (lo que los
    // scripts importen en runtime lo agrega W3dImportW3DAppendix por su cuenta)
    W3dRigidClear();
    W3dRigidEnsure(SceneCollection);
    W3dHitboxClear();   // la partida arranca sin nadie "adentro" de ningun hitbox
    // MULTI-ESCENA: registrar las escenas UI. La INICIAL arranca ya; las demas quedan
    // cargadas pero sin inicir (init perezoso al hacer changeScene()).
    W3dSceneSetInit(EditorInitScene);
    W3dSceneRegisterAll();
    bool hasScenes = W3dSceneHasScenes();
    // los objetos con script que NO cuelgan de ninguna escena UI (3D, gamepad, etc.) NO
    // son parte del sistema de escenas: se inician SIEMPRE (comportamiento viejo). Los de
    // escenas UI los inicia la escena (la inicial ahora; el resto, perezoso).
    for (size_t i = 0; i < gScripted.size(); i++) {
        Object* s = gScripted[i];
        Object* root = W3dSceneRootOf(s);
        bool inScene = hasScenes && root && root->getType() == ObjectType::ui;
        if (inScene) continue;
        if (!W3dScriptLoad(s)) continue;
        SimReresolve(s);
        W3dScriptStart(s);
    }
    if (hasScenes) W3dSceneStart();   // muestra + inicia SOLO la escena inicial
    // SEGUNDA pasada de cuerpos rigidos: lo que los start() importaron con
    // importW3D() entro ANTES de gActive=true, asi que el hook del anexo
    // no lo dio de alta (Asegurar es idempotente: los ya creados no se tocan).
    W3dRigidEnsure(SceneCollection);
}

// changeScene() a una ESCENA o un JUEGO 3D del proyecto, con la partida andando (lo aplica W3dEscena
// al final del frame). La raiz actual SUELTA su partida (scripts, rigidos, hitbox, animaciones:
// getShared() sobrevive, es lo que pasa datos entre niveles) y la pedida arranca DE CERO: si ya se
// jugo en esta partida vuelve a su base, igual que el juego compilado, que la recarga de su entrada.
// Un JUEGO arranca su simulacion; una ESCENA ademas reproduce su animacion desde el inicio (una
// cinematica: sus scripts pueden mirar animSceneCurrent() y pedir la siguiente al terminar).
static void CollectSub(Object* o, std::vector<Object*>& out);   // (abajo: lo que se libera sale de las fotos)
static void PruneSnapshots(const std::set<Object*>& outside);
static void SimChangeRoot(int idx) {
    if (!gActive) return;
    // el MODO de la partida (el timeline en "Juego") sigue en la raiz nueva aunque sea una escena:
    // lo que corre es el juego. El Stop vuelve a la raiz del Play y cada una recupera el suyo.
    const int kind = ActiveAnimKind; const bool isGame = AnimIsGame; const int start = StartFrame;
    // LAS CAPAS (escenas cargadas encima: loadScene, W3dScene.h) siguen corriendo: sus scripts no se descargan y
    // las 3D pasan a la raiz nueva (las 2D quedan en la suya: son del usuario; se dibujan y corren desde la lista)
    std::vector<Object*> scriptedLayers;
    for (size_t i = 0; i < gScripted.size(); i++)
        if (W3dSceneInLayer(gScripted[i])) scriptedLayers.push_back(gScripted[i]); else W3dScriptUnloadFrom(gScripted[i]);
    for (size_t i = 0; i < gScriptedNew.size(); i++)
        if (W3dSceneInLayer(gScriptedNew[i])) scriptedLayers.push_back(gScriptedNew[i]); else W3dScriptUnloadFrom(gScriptedNew[i]);
    gScripted.clear();
    gScriptedNew.clear();
    std::vector<Object*> layers3D;
    W3dSceneLayersDetach(false, layers3D);
    W3dPhysicsClear();
    W3dRigidClear();
    W3dHitboxClear();
    W3dAnimSceneReset();
    W3dAnimObjectsReset();   // (los clips de objeto de la que se va dejan de sonar)
    BindsGameResetPointers();
    W3dSceneClear();   // (las escenas UI de la que se va vuelven a verse, como las dejo el usuario)
    Object* old = SceneCollection;
    const int oldIdx = W3dActiveRoot();
    if (!W3dRootChangeActive(idx)) {
        w3dLogfE("[juego] cambiarEscena: no pude abrir la escena %d", idx);
    } else {
        ActiveAnimKind = kind; AnimIsGame = isGame; StartFrame = start;
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
        // la base de la escena destino: la del usuario (se toma la primera vez que se entra)
        Object* r = SceneCollection;
        bool already = false;
        if (r == gRootPlay) { Apply(gBaseEditor); already = true; }
        for (size_t i = 0; i < gBasesRoots.size() && !already; i++)
            if (gBasesRoots[i].root == r) { Apply(gBasesRoots[i].snap); already = true; }
        if (!already) {
            W3dStreamingSessionPrepare();   // (sus diferidas, como al arrancar el Play: antes de la foto)
            gBasesRoots.push_back(SimBaseRoot()); gBasesRoots.back().root = r; Snapshot(&gBasesRoots.back().snap);
        }
    }
    W3dSceneLayersHang(SceneCollection, layers3D);   // (las capas 3D, arriba de todo en la raiz nueva)
    { extern void PropsForgetScene(); PropsForgetScene(); }
    // la raiz que se deja, si la cargo la PARTIDA (no el usuario) y no es la del Play: se DESCARGA (en el N95 la
    // cinematica y el area 1 juntas no entran en la RAM). Antes, fuera de todas las listas y fotos del Play. (No si
    // ahi vive una escena 2D cargada encima: sigue en pantalla)
    if (old && old != SceneCollection && old != gRootPlay && !gUserRoots.count(old) &&
        !W3dSceneLayerInsideOf(old)) {
        std::vector<Object*> sub;
        CollectSub(old, sub);
        std::set<Object*> outside(sub.begin(), sub.end());
        // (lo que un script destruyo ahi y espera el Stop descolgado: se va con su raiz)
        std::vector<Object*> unhooked;
        for (size_t k = gDestroyedInPlay.size(); k-- > 0; ) {
            bool inside = false;
            for (Object* p = gDestroyedInPlay[k].parent; p && !inside; p = p->Parent) inside = outside.count(p) != 0;
            if (inside) { unhooked.push_back(gDestroyedInPlay[k].o); gDestroyedInPlay.erase(gDestroyedInPlay.begin() + (long)k); }
        }
        for (size_t i = 0; i < unhooked.size(); i++) {
            std::vector<Object*> ds;
            CollectSub(unhooked[i], ds);
            for (size_t j = 0; j < ds.size(); j++) { outside.insert(ds[j]); W3dLiveObjectMark(ds[j], true); }
        }
        PruneSnapshots(outside);
        for (size_t i = gCreatedInPlay.size(); i-- > 0; )
            if (outside.count(gCreatedInPlay[i].o)) gCreatedInPlay.erase(gCreatedInPlay.begin() + (long)i);
        for (size_t i = gBasesRoots.size(); i-- > 0; )
            if (gBasesRoots[i].root == old) gBasesRoots.erase(gBasesRoots.begin() + (long)i);
        for (size_t i = ObjSelects.size(); i-- > 0; ) if (outside.count(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + (long)i);
        if (outside.count(ActiveObj)) ActiveObj = NULL;
        gPruning = true;   // (ya esta fuera de las listas y las fotos: el gancho del destructor no repite)
        for (size_t i = 0; i < unhooked.size(); i++) W3dFreeSubtree(unhooked[i]);
        W3dRootUnloadFromGame(oldIdx);
        gPruning = false;
    }
    Traverse(SceneCollection, NULL, &gScripted);
    // (lo de las capas ya corre: no se vuelve a arrancar)
    for (size_t i = gScripted.size(); i-- > 0; ) if (W3dSceneInLayer(gScripted[i])) gScripted.erase(gScripted.begin() + (long)i);
    StartCurrentRoot();
    gScripted.insert(gScripted.end(), scriptedLayers.begin(), scriptedLayers.end());
    W3dStreamingSessionStart();   // (las diferidas cerca de la escena nueva, como al arrancar el Play)
    W3dRootStartCinematic();      // (una ESCENA: su animacion desde el inicio; un juego no hace nada)
    w3dLogf("[juego] cambiarEscena: ahora corre '%s' (%d objeto(s) con script)",
            W3dActiveRoot() >= 0 ? W3dRoots()[(size_t)W3dActiveRoot()].name.c_str() : "?", (int)gScripted.size());
    g_redraw = true;
}

// lo que el Play hace en cada momento de una CINEMATICA (playScene, W3dRoots.h). La escena ya es
// la raiz activa en los tres: su foto antes de animarla, sus scripts al arrancar y, al salir, sus scripts
// afuera y la escena de vuelta como la dejo el usuario (el juego lo repone el swap de raiz: no se toco).
static void SimCinematic(int moment) {
    if (moment == W3D_CINEMATIC_BEFORE) {
        Snapshot(&gCinematicBase);
        gCinematicScripted.clear();
        { extern void PropsForgetScene(); PropsForgetScene(); }
    } else if (moment == W3D_CINEMATIC_ENTERED) {
        Traverse(SceneCollection, NULL, &gCinematicScripted);
        for (size_t i = 0; i < gCinematicScripted.size(); i++) {
            Object* o = gCinematicScripted[i];
            if (!W3dScriptLoad(o)) continue;
            SimReresolve(o);
            W3dScriptStart(o);
        }
    } else {
        for (size_t i = 0; i < gCinematicScripted.size(); i++) W3dScriptUnloadFrom(gCinematicScripted[i]);
        gCinematicScripted.clear();
        Apply(gCinematicBase);
        gCinematicBase = SimSnap();
        { extern void PropsForgetScene(); PropsForgetScene(); }
    }
}

// ---- instantiate(): lo nuevo arranca como si hubiera estado desde el Play (sus scripts cargan, resuelven sus
// refs e inician; sus cuerpos rigidos se crean; sus hitbox participan solos desde el proximo paso). Queda
// anotado para borrarlo al Stop
static void SimNewObject(Object* root) {
    // (tambien mientras se ARRANCA: un spawner hace instantiate() en su start())
    if ((!gActive && !gStarting) || !root) return;
    SimCreated c; c.o = root; c.serial = root->serial;
    gCreatedInPlay.push_back(c);
    // los CUERPOS RIGIDOS antes que los start(), igual que al arrancar la partida (StartCurrentRoot): un
    // physicsVel()/physicsImpulse() en el start() de lo instanciado encuentra su cuerpo (si no, se perdia)
    W3dRigidEnsure(root);
    std::vector<Object*> scripted;
    Traverse(root, NULL, &scripted);
    for (size_t i = 0; i < scripted.size(); i++) {
        Object* o = scripted[i];
        if (!W3dScriptLoad(o)) continue;
        SimReresolve(o);
        W3dScriptStart(o);
        gScriptedNew.push_back(o);
    }
    W3dRigidEnsure(root);   // (lo que esos start() importaron: Asegurar es idempotente)
    g_redraw = true;
}

// lo que las fotos del Play (la escena del usuario, la base de una cinematica y la de cada escena por
// la que paso la partida) tienen de objetos que se LIBERAN
// (y lo que las OTRAS entradas nombran de ellos: el RIEL de una camara que setRail() puso en una curva que se
// libera. Sin esto, restaurar la foto -Aplicar- le devolvia a la camara una curva liberada)
static void PruneSnap(SimSnap& s, const std::set<Object*>& outside) {
    for (size_t i = s.ents.size(); i-- > 0; ) if (outside.count(s.ents[i].o)) s.ents.erase(s.ents.begin() + (long)i);
    for (size_t i = 0; i < s.ents.size(); i++) if (s.ents[i].rail && outside.count((Object*)s.ents[i].rail)) s.ents[i].rail = NULL;
    for (size_t i = s.arms.size(); i-- > 0; ) if (outside.count((Object*)s.arms[i].a)) s.arms.erase(s.arms.begin() + (long)i);
    for (size_t i = s.hier.size(); i-- > 0; ) if (outside.count(s.hier[i].o)) s.hier.erase(s.hier.begin() + (long)i);
}
// TODAS las fotos de la partida
static void PruneSnapshots(const std::set<Object*>& outside) {
    PruneSnap(gBaseEditor, outside);
    PruneSnap(gCinematicBase, outside);
    for (size_t i = 0; i < gBasesRoots.size(); i++) PruneSnap(gBasesRoots[i].snap, outside);
}
static void RemoveFromList(std::vector<Object*>& v, const std::set<Object*>& outside) {
    for (size_t i = v.size(); i-- > 0; ) if (outside.count(v[i])) v.erase(v.begin() + (long)i);
}
static void CollectSub(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectSub(o->Childrens[i], out);
}
// 'o' (o algun ancestro suyo) nacio en este Play?
static bool CreatedInPlay(Object* o) {
    for (Object* p = o; p; p = p->Parent)
        for (size_t i = 0; i < gCreatedInPlay.size(); i++)
            if (gCreatedInPlay[i].o == p && gCreatedInPlay[i].serial == p->serial) return true;
    return false;
}
// ---- destroy() (al final del frame): sale de las listas del Play; lo creado jugando se LIBERA, lo del usuario
// se DESCUELGA (el Stop lo devuelve a su lugar)
static void SimObjectDestroy(Object* o) {
    if (!o) return;
    std::vector<Object*> sub;
    CollectSub(o, sub);
    std::set<Object*> outside(sub.begin(), sub.end());
    RemoveFromList(gScripted, outside);
    RemoveFromList(gScriptedNew, outside);
    RemoveFromList(gCinematicScripted, outside);
    gLuaMsAccum.clear();   // (paralelo a gScripted: el proximo tick lo rearma)
    if (CreatedInPlay(o)) {
        PruneSnapshots(outside);
        for (size_t i = gCreatedInPlay.size(); i-- > 0; )
            if (outside.count(gCreatedInPlay[i].o)) gCreatedInPlay.erase(gCreatedInPlay.begin() + (long)i);
        gPruning = true;   // (las listas y las fotos ya estan podadas: el gancho del destructor no repite)
        W3dScriptDestroyObject(o);
        gPruning = false;
        return;
    }
    // del USUARIO: afuera del arbol hasta el Stop (vivo: las fotos lo siguen nombrando sin peligro)
    for (size_t i = 0; i < sub.size(); i++) {
        W3dScriptUnloadFrom(sub[i]);
        W3dRigidForget(sub[i]);
        W3dHitboxForget(sub[i]);
        W3dPhysicsForget(sub[i]);
        W3dLiveObjectMark(sub[i], false);   // (para lua ya no existe, como en el juego compilado)
    }
    // ...y nadie lo sigue teniendo en object("x"): object() da nil, igual que en el juego compilado (que lo
    // libera con W3dScriptDestroyObject). El Stop descarga todos los scripts: no hay nada que devolver
    W3dScriptReleaseRefsA(sub);
    for (size_t i = ObjSelects.size(); i-- > 0; ) if (outside.count(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + (long)i);
    if (outside.count(ActiveObj)) ActiveObj = NULL;
    SimDestroyed d;
    d.o = o; d.serial = o->serial;
    d.parent = o->Parent; d.parentSerial = o->Parent ? o->Parent->serial : 0; d.index = -1;
    if (o->Parent) {
        std::vector<Object*>& ch = o->Parent->Childrens;
        for (size_t i = 0; i < ch.size(); i++) if (ch[i] == o) { d.index = (int)i; ch.erase(ch.begin() + (long)i); break; }
    }
    gDestroyedInPlay.push_back(d);
    g_redraw = true;
}
// ---- el STREAMING (io/Streaming.h) descarga lo que genero una instancia: se LIBERA siempre, tambien lo del usuario
// (lo generado sale de la definicion: el streaming lo anota y el Stop lo vuelve a generar). Como destroy() de lo creado
// jugando: fuera de las listas y de las fotos, y W3dScriptDestroyObject
static void SimObjectFree(Object* o) {
    if (!o) return;
    std::vector<Object*> sub;
    CollectSub(o, sub);
    std::set<Object*> outside(sub.begin(), sub.end());
    // lo que un script DESTRUYO adentro (descolgado, esperando el Stop para volver a su padre): su padre se va, asi que
    // se va con el (es lo generado de la misma instancia: el Stop la regenera entera)
    std::vector<Object*> unhooked;
    for (size_t k = gDestroyedInPlay.size(); k-- > 0; )
        if (outside.count(gDestroyedInPlay[k].parent)) {
            unhooked.push_back(gDestroyedInPlay[k].o);
            gDestroyedInPlay.erase(gDestroyedInPlay.begin() + (long)k);
        }
    for (size_t i = 0; i < unhooked.size(); i++) {
        std::vector<Object*> ds;
        CollectSub(unhooked[i], ds);
        for (size_t j = 0; j < ds.size(); j++) { outside.insert(ds[j]); W3dLiveObjectMark(ds[j], true); }
    }
    RemoveFromList(gScripted, outside);
    RemoveFromList(gScriptedNew, outside);
    RemoveFromList(gCinematicScripted, outside);
    gLuaMsAccum.clear();   // (paralelo a gScripted: el proximo tick lo rearma)
    PruneSnapshots(outside);
    for (size_t i = gCreatedInPlay.size(); i-- > 0; )
        if (outside.count(gCreatedInPlay[i].o)) gCreatedInPlay.erase(gCreatedInPlay.begin() + (long)i);
    gPruning = true;   // (las listas y las fotos ya estan podadas: el gancho del destructor no repite)
    W3dScriptDestroyObject(o);
    for (size_t i = 0; i < unhooked.size(); i++) W3dFreeSubtree(unhooked[i]);   // (ya sin padre)
    gPruning = false;
}
// al Stop: lo DESTRUIDO del usuario vuelve a su lugar (al reves: los indices guardados valen) y lo CREADO jugando
// se libera
static void SimUndoCreatedAndDestroyed() {
    for (size_t k = gDestroyedInPlay.size(); k-- > 0; ) {
        SimDestroyed& d = gDestroyedInPlay[k];
        // (lo descolgado es NUESTRO hasta aca: nadie mas lo pudo liberar; vuelve a ser un objeto vivo)
        std::vector<Object*> sub;
        CollectSub(d.o, sub);
        for (size_t i = 0; i < sub.size(); i++) W3dLiveObjectMark(sub[i], true);
        if (!d.parent) continue;
        std::vector<Object*>& ch = d.parent->Childrens;
        const int idx = (d.index < 0 || d.index > (int)ch.size()) ? (int)ch.size() : d.index;
        ch.insert(ch.begin() + idx, d.o);
        d.o->Parent = d.parent;
    }
    gDestroyedInPlay.clear();
    gPruning = true;   // (la partida se termina: el gancho del destructor no poda nada)
    for (size_t k = gCreatedInPlay.size(); k-- > 0; ) {
        SimCreated c = gCreatedInPlay[k];
        if (W3dLiveObjectSerial(c.o, c.serial)) W3dScriptDestroyObject(c.o);
    }
    gPruning = false;
    gCreatedInPlay.clear();
    gScriptedNew.clear();
}

// ============================================================================
//  GUARDAR CON LA PARTIDA ANDANDO (io/SaveW3D.cpp): el archivo lleva la escena del USUARIO. Mientras dura el
//  guardado, lo que un script destruyo vuelve a colgar de su padre (en su lugar) y lo que instantiate() creo sale
//  del arbol; SimSavedEnd deja las dos cosas exactamente como estaban y la partida sigue (no corre ningun tick
//  en el medio). Sin partida no hacen nada.
// ============================================================================
struct SimRemovedSave { Object* o; Object* parent; int index; };
static std::vector<SimRemovedSave> gRemovedSave;
static bool gSavingInPlay = false;
static int IndexInChildren(Object* parent, Object* o) {
    for (size_t i = 0; parent && i < parent->Childrens.size(); i++) if (parent->Childrens[i] == o) return (int)i;
    return -1;
}
void SimSavedStart() {
    if (!gActive || gSavingInPlay) return;
    gSavingInPlay = true;
    // lo DESTRUIDO del usuario, a su lugar (al reves, como el Stop: los indices guardados valen) y VIVO otra vez
    // mientras tanto (el guardado sincroniza los overrides de una instancia comparando lo que genero: con sus
    // objetos "muertos" los perdia)
    for (size_t k = gDestroyedInPlay.size(); k-- > 0; ) {
        SimDestroyed& d = gDestroyedInPlay[k];
        std::vector<Object*> sub;
        CollectSub(d.o, sub);
        for (size_t i = 0; i < sub.size(); i++) W3dLiveObjectMark(sub[i], true);
        if (!d.parent || IndexInChildren(d.parent, d.o) >= 0) continue;
        std::vector<Object*>& ch = d.parent->Childrens;
        const int idx = (d.index < 0 || d.index > (int)ch.size()) ? (int)ch.size() : d.index;
        ch.insert(ch.begin() + idx, d.o);
        d.o->Parent = d.parent;
    }
    // lo CREADO jugando, afuera (de adelante hacia atras: se vuelve a colgar al reves)
    gRemovedSave.clear();
    for (size_t i = 0; i < gCreatedInPlay.size(); i++) {
        const SimCreated& c = gCreatedInPlay[i];
        if (!W3dLiveObjectSerial(c.o, c.serial)) continue;
        Object* p = c.o->Parent;
        const int idx = IndexInChildren(p, c.o);
        if (idx < 0) continue;
        p->Childrens.erase(p->Childrens.begin() + idx);
        SimRemovedSave s; s.o = c.o; s.parent = p; s.index = idx;
        gRemovedSave.push_back(s);
    }
}
void SimSavedEnd() {
    if (!gSavingInPlay) return;
    gSavingInPlay = false;
    for (size_t k = gRemovedSave.size(); k-- > 0; ) {
        SimRemovedSave& s = gRemovedSave[k];
        std::vector<Object*>& ch = s.parent->Childrens;
        const int idx = (s.index > (int)ch.size()) ? (int)ch.size() : s.index;
        ch.insert(ch.begin() + idx, s.o);
    }
    gRemovedSave.clear();
    for (size_t k = 0; k < gDestroyedInPlay.size(); k++) {
        SimDestroyed& d = gDestroyedInPlay[k];
        const int idx = IndexInChildren(d.parent, d.o);
        if (idx >= 0) d.parent->Childrens.erase(d.parent->Childrens.begin() + idx);
        std::vector<Object*> sub;
        CollectSub(d.o, sub);
        for (size_t i = 0; i < sub.size(); i++) W3dLiveObjectMark(sub[i], false);   // (para lua sigue sin existir)
    }
}

// ============================================================================
//  UN OBJETO SE LIBERA CON LA PARTIDA ANDANDO por un camino que no es destroy(): un paso del historial que se
//  cae con lo que tenia borrado, algo que el editor regenera o descarga... Las listas del Play (los objetos con
//  script, los nuevos, los de una cinematica) y sus fotos (la escena del
//  usuario, las bases de cada escena) no pueden quedar apuntandolo: el tick siguiente leia memoria liberada.
//  Es un piso de memoria (el editor igual bloquea lo que regenera con el Play andando). Lo engancha el
//  destructor de Object (W3dUnlinkRegister, Objects.h); sin partida no hace nada.
// ============================================================================
static void SimUnlink(Object* deleted) {
    // (guardando: lo unico que se libera son las raices que el guardado cargo solo para escribirlas, que no estaban
    //  cargadas en la partida y no estan en ninguna lista ni foto; se saltea para no recorrer las fotos por cada una)
    if (!gActive || gPruning || gSavingInPlay || !deleted) return;
    std::set<Object*> outside;
    outside.insert(deleted);
    RemoveFromList(gScripted, outside);
    RemoveFromList(gScriptedNew, outside);
    RemoveFromList(gCinematicScripted, outside);
    gLuaMsAccum.clear();   // (paralelo a gScripted: el proximo tick lo rearma)
    PruneSnapshots(outside);
    for (size_t i = gCreatedInPlay.size(); i-- > 0; )
        if (gCreatedInPlay[i].o == deleted) gCreatedInPlay.erase(gCreatedInPlay.begin() + (long)i);
    // lo del usuario que un script destruyo espera el Stop colgado de su padre: si el padre se fue, vuelve a la
    // raiz del Play (al final)
    for (size_t i = 0; i < gDestroyedInPlay.size(); i++)
        if (gDestroyedInPlay[i].parent == deleted) { gDestroyedInPlay[i].parent = gRootPlay; gDestroyedInPlay[i].index = -1; }
}
namespace {
struct SimHookUnlink { SimHookUnlink() { W3dUnlinkRegister(SimUnlink); } } gSimHookUnlink;
}

// LAS ESCENAS ABIERTAS en el editor (W3dRoots.h): el Play arranca con la PRIMERA de la lista (la base) y carga las demas
// encima, en su orden, como loadScene(). Si se estaba editando otra, la primera pasa a ser la activa y el Stop vuelve
// a la que se editaba. El ojo del outliner es solo del editor: la base se ve jugando aunque estuviera oculta
static std::string gActivePrevOfPlay;
static Object* gBaseEyeRoot = NULL;
static bool gBaseEye = true;
static void SimReturnToEdited() {
    if (gActivePrevOfPlay.empty()) return;
    const int k = W3dRootFind(W3D_ROOT_SCENE, gActivePrevOfPlay);
    gActivePrevOfPlay.clear();
    std::string reason;
    if (k < 0 || !W3dRootIsOpen(k)) return;
    if (!W3dActivateRoot(k, &reason)) w3dLogfW("[play] no se pudo volver a '%s': %s", W3dRoots()[(size_t)k].name.c_str(),
                                              reason.c_str());
}
static void SimPlay() {
    // volcar a disco/overlay los .lua editados en el IDE (sucios) ANTES de leerlos: sino Play corre el .lua VIEJO
    // (el cambio vivia solo en el buffer del IDE). Asi cambiar un valor en el script y dar Play YA se ve.
    { extern void IDESaveDirty(); IDESaveDirty(); }
    // LA RED: un proyecto que juega en red sin el permiso decidido pregunta antes de arrancar (y arranca al
    // responder). Ver getConfig/W3dNetPermissions.h
    { extern bool NetCanPlay(); if (!NetCanPlay()) return; }
    std::vector<std::string> layersOfPlay;
    {
        std::vector<int> ab;
        W3dOpenRoots(ab);
        const std::vector<W3dRootRow>& fs = W3dRoots();
        if (ab.size() >= 2 && ab[0] != W3dActiveRoot()) {
            const int act = W3dActiveRoot();
            std::string reason;
            const bool playing = PlayAnimation;   // (cambiar de escena para la reproduccion: el Play la sigue)
            const bool ok = W3dActivateRoot(ab[0], &reason);
            PlayAnimation = playing;
            if (!ok) {   // (el Play no arranca: que no lo reintente en cada cuadro)
                Notify(T(reason.c_str()), true);
                PlayAnimation = false;
                return;
            }
            gActivePrevOfPlay = (act >= 0 && act < (int)fs.size()) ? fs[(size_t)act].name : std::string();
        }
        for (size_t i = 1; i < ab.size(); i++) layersOfPlay.push_back(fs[(size_t)ab[i]].name);
    }
    // ...y la vista de un clip de jerarquia que se estaba editando vuelve a su clip: lo que animObject()
    // reproduce es la biblioteca (compartida), no la vista
    W3dClipsViewsSync();
    gScripted.clear();
    Traverse(SceneCollection, NULL, &gScripted);   // en el orden del arbol (outliner)
    // sin scripts no hay partida... salvo en un JUEGO, que simula igual (fisica, particulas, hitbox), o con otras
    // escenas abiertas encima (sus scripts)
    if (gScripted.empty() && !W3dActiveRootIsGame() && layersOfPlay.empty()) { SimReturnToEdited(); return; }
    // STREAMING (io/Streaming.h): las instancias DIFERIDAS sueltan lo que generaron (quedan como en el juego compilado
    // recien abierto: el streaming las carga jugando) ANTES de la foto y de juntar los scripts; el Stop las repone
    W3dStreamingSessionPrepare();
    gScripted.clear();
    Traverse(SceneCollection, NULL, &gScripted);
    // snapshot de la escena DEL USUARIO (antes de que start() toque nada): es lo
    // que Stop restaura. (Antes se pisaba con el snapshot post-inicio de abajo y
    // Stop dejaba pegado lo que los scripts movieron en start().)
    gBaseEyeRoot = SceneCollection;
    gBaseEye = SceneCollection->visible;
    SceneCollection->visible = true;
    Snapshot(&gBaseEditor);
    gScriptsBase.clear();   // (y lo de los scripts: el inspector, el ojo; ver ScriptsSnapshot)
    ScriptsSnapshot(SceneCollection);
    for (size_t i = 0; i < W3dRoots().size(); i++)
        if (W3dRoots()[i].root && W3dRoots()[i].root != SceneCollection) ScriptsSnapshot(W3dRoots()[i].root);
    gRootPlay = SceneCollection;
    gBasesRoots.clear();
    gUserRoots.clear();
    for (size_t i = 0; i < W3dRoots().size(); i++) if (W3dRoots()[i].root) gUserRoots.insert(W3dRoots()[i].root);
    W3dRootsSetChangeGame(SimChangeRoot);   // changeScene() a otra escena 3D
    W3dRootsSetCinematic(SimCinematic);                  // playScene(): una cinematica con el juego en pausa
    // instantiate() / destroy() de lua: lo nuevo arranca, lo destruido se va (y el Stop lo arregla)
    gCreatedInPlay.clear(); gDestroyedInPlay.clear(); gScriptedNew.clear();
    W3dScriptSetNewObject(SimNewObject);
    W3dScriptSetObjectDestroy(SimObjectDestroy);
    W3dScriptSetObjectFree(SimObjectFree);   // (lo que descarga el streaming)
    W3dAnimObjectsReset();                      // (ningun clip de objeto sonando de una partida anterior)
    // las BASES del mix de escenas (la transform de cada objeto antes de mezclarlo): Stop las
    // repone, asi un objeto que el mix toca por primera vez jugando no se queda con la base
    // de la partida para el Mix del editor
    W3dMixScenesBasesSave();
    // (lo que un start() instancia ya es de la partida: sus scripts arrancan y el Stop lo borra)
    gStarting = true;
    StartCurrentRoot();
    // ...y las diferidas que estan CERCA se generan ya (bloqueante: el primer frame esta completo) y arrancan como lo
    // instanciado, despues de los start() de la escena: igual que en el juego compilado (W3dGameStart)
    W3dStreamingSessionStart();
    // el Play de una raiz de tipo ESCENA (solo desde el harness o un proyecto armado a mano: la UI
    // simula los JUEGOS) arranca como en el juego compilado: con su animacion desde el inicio
    W3dRootStartCinematic();
    gStarting = false;
    // ...y las otras escenas abiertas, encima (se aplica al final del cuadro, como un loadScene de un start())
    for (size_t i = 0; i < layersOfPlay.size(); i++) W3dSceneRequestLoad(layersOfPlay[i]);
    if (!layersOfPlay.empty()) w3dLogf("[play] base '%s' + %d escena(s) encima", W3dActiveRoot() >= 0 ?
        W3dRoots()[(size_t)W3dActiveRoot()].name.c_str() : "", (int)layersOfPlay.size());
    gTick = 0;
    gActive = true;
    CurrentFrame = StartFrame;
}

// re-resuelve TODO lo asignado en el editor para los scripts de 'o' (tambien se llama
// al editar una propiedad con el juego andando: el cambio se ve al instante)
void SimReresolve(Object* o) {
    if (!o || !o->scriptData) return;
    for (size_t e = 0; e < o->scriptData->scripts.size(); e++) {
        const W3dScriptEntry& ent = o->scriptData->scripts[e];
        for (size_t r = 0; r < ent.refs.size(); r++) {
            const std::string& prop = ent.refs[r].first;
            const std::string& val  = ent.refs[r].second;
            // refs POR ESCENA: buscar dentro de la raiz de la escena del dueno (no en toda
            // SceneCollection) para que no colisionen nombres repetidos entre escenas.
            // (y adentro de una instancia de prefab, primero lo de SU instancia: W3dSceneFindRef)
            Object* obj = W3dSceneFindRef(o, val);
            if (obj) W3dScriptResolveRef(o, (int)e, prop, obj);
            W3dScriptResolveOption(o, (int)e, prop, val);
            // el MISMO par sirve para las propiedades de VALOR (tipo 2): el que
            // corresponda lo decide el .lua al leer (object/option/property)
            W3dScriptResolveValue(o, (int)e, prop, val);
        }
    }
}

// la ENTRADA del pad (y las flechas del teclado) hacia los scripts: la leen el tick de la partida y el de
// una cinematica (un "saltar" con un boton)
static void EntryPad() {
    // el GAMEPAD analogico entra a los scripts (los ejes ya vienen con deadzone). En Symbian los joysticks del driver
    // Bluetooth (GamepadsSymbianTick) dejan su estado en los MISMOS arreglos: de aca para abajo es igual.
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
        const int dl = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT]  || gArrow[2]) ? 1 : 0;
        const int dr = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] || gArrow[3]) ? 1 : 0;
        const int du = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP]    || gArrow[0]) ? 1 : 0;
        const int dd = (buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN]  || gArrow[1]) ? 1 : 0;
        // el eje Y de SDL crece HACIA ABAJO, igual que el del stick: arriba = -1
        lx = (float)(dr - dl); ly = (float)(dd - du);
    }
    W3dScriptStick(0, lx, ly);
    W3dScriptStick(1, axisState[SDL_CONTROLLER_AXIS_RIGHTX], axisState[SDL_CONTROLLER_AXIS_RIGHTY]);
    // los GATILLOS analogicos (LT / RT): pedales de un juego de autos (trigger() en lua, 0..1)
    W3dScriptTrigger(0, axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]);
    W3dScriptTrigger(1, axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]);
    W3dScriptButtonPad("a", buttonState[SDL_CONTROLLER_BUTTON_A]);
    W3dScriptButtonPad("b", buttonState[SDL_CONTROLLER_BUTTON_B]);
    W3dScriptButtonPad("x", buttonState[SDL_CONTROLLER_BUTTON_X]);
    W3dScriptButtonPad("y", buttonState[SDL_CONTROLLER_BUTTON_Y]);
    // ...y ademas por su nombre, para el script que los quiera como botones sueltos
    // (menus, "pausa con arriba/abajo"). Mismos nombres que las flechas del teclado.
    W3dScriptButtonPad("up",     buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP]);
    W3dScriptButtonPad("down",      buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN]);
    W3dScriptButtonPad("left",  buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT]);
    W3dScriptButtonPad("right",    buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]);
    // los hombros, los gatillos como botones, los sticks apretados, start y select (con los nombres de un mando de
    // PlayStation: L1/R1 = los hombros, L2/R2 = los gatillos; en un mando de Xbox son LB/RB y LT/RT)
    W3dScriptButtonPad("l1",     buttonState[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]);
    W3dScriptButtonPad("r1",     buttonState[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]);
    W3dScriptButtonPad("l2",     axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]  > 0.5f);
    W3dScriptButtonPad("r2",     axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > 0.5f);
    W3dScriptButtonPad("l3",     buttonState[SDL_CONTROLLER_BUTTON_LEFTSTICK]);
    W3dScriptButtonPad("r3",     buttonState[SDL_CONTROLLER_BUTTON_RIGHTSTICK]);
    W3dScriptButtonPad("start",  buttonState[SDL_CONTROLLER_BUTTON_START]);
    W3dScriptButtonPad("select", buttonState[SDL_CONTROLLER_BUTTON_BACK]);
    // y CADA MANDO por separado (button("a", 2), stick("izq", 2)...: la pantalla dividida). Igual que la mezcla, sin
    // las flechas del teclado: el teclado lo reparte el juego (cada jugador con sus teclas)
    static const struct { const char* n; int b; } kButtons[] = {
        { "a", SDL_CONTROLLER_BUTTON_A }, { "b", SDL_CONTROLLER_BUTTON_B }, { "x", SDL_CONTROLLER_BUTTON_X },
        { "y", SDL_CONTROLLER_BUTTON_Y }, { "up", SDL_CONTROLLER_BUTTON_DPAD_UP },
        { "down", SDL_CONTROLLER_BUTTON_DPAD_DOWN }, { "left", SDL_CONTROLLER_BUTTON_DPAD_LEFT },
        { "right", SDL_CONTROLLER_BUTTON_DPAD_RIGHT }, { "l1", SDL_CONTROLLER_BUTTON_LEFTSHOULDER },
        { "r1", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER }, { "l3", SDL_CONTROLLER_BUTTON_LEFTSTICK },
        { "r3", SDL_CONTROLLER_BUTTON_RIGHTSTICK }, { "start", SDL_CONTROLLER_BUTTON_START },
        { "select", SDL_CONTROLLER_BUTTON_BACK } };
    for (int m = 0; m < W3D_GAMEPADS_SDL; m++) {
        const float* ax = axisStateM[m];
        const bool* bt = buttonStateM[m];
        float mx = ax[SDL_CONTROLLER_AXIS_LEFTX], my = ax[SDL_CONTROLLER_AXIS_LEFTY];
        if (mx == 0.0f && my == 0.0f) {
            mx = (float)((bt[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] ? 1 : 0) - (bt[SDL_CONTROLLER_BUTTON_DPAD_LEFT] ? 1 : 0));
            my = (float)((bt[SDL_CONTROLLER_BUTTON_DPAD_DOWN] ? 1 : 0) - (bt[SDL_CONTROLLER_BUTTON_DPAD_UP] ? 1 : 0));
        }
        W3dScriptStickOf(m, 0, mx, my);
        W3dScriptStickOf(m, 1, ax[SDL_CONTROLLER_AXIS_RIGHTX], ax[SDL_CONTROLLER_AXIS_RIGHTY]);
        W3dScriptTriggerOf(m, 0, ax[SDL_CONTROLLER_AXIS_TRIGGERLEFT]);
        W3dScriptTriggerOf(m, 1, ax[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]);
        for (size_t k = 0; k < sizeof(kButtons) / sizeof(kButtons[0]); k++)
            W3dScriptButtonPadOf(m, kButtons[k].n, bt[kButtons[k].b]);
        W3dScriptButtonPadOf(m, "l2", ax[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > 0.5f);
        W3dScriptButtonPadOf(m, "r2", ax[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > 0.5f);
    }
}
// UN TICK DE CINEMATICA (playScene): el juego en PAUSA (ni su fisica ni sus scripts ni sus
// animaciones: su arbol ni esta en el activo) y corre la ESCENA: su animacion, sus esqueletos, sus clips
// de objeto y sus scripts. No avanza el tick de la partida.
static void TickCinematic(float dt) {
    { extern void W3dCamerasRailTick(); W3dCamerasRailTick(); }
    EntryPad();
    for (size_t i = 0; i < gCinematicScripted.size(); i++)
        if (gCinematicScripted[i]->visible) W3dScriptUpdate(gCinematicScripted[i], dt);
    W3dCinematicTick(dt);
    W3dAnimObjectsTick(dt);
    W3dArmaturesGameTick(dt);
    BindsGameSnapshotPointers();
    W3dScriptEndFrame();
    // la vuelta al juego (termino / stopScene3D) y su onFinish(), al final del frame
    W3dSceneApplyPending();
    if (!PlayAnimation) UpdateAnimations(dt);
    g_redraw = true;
}

// DESGLOSE de la logica por fase (ms acumulados desde la ultima lectura): fisica de cuerpos rigidos, hitbox, lua,
// animaciones (escena + objetos + esqueletos) y streaming. Lo leen el [PERF] del N95 y benchjuego en PC: dice a que
// se le va el tiempo de la logica sin adivinar
static double gSimMs[SIM_PERF_N] = { 0, 0, 0, 0, 0 };
static int    gSimTicks = 0;
int SimPerfBreakdown(double out[SIM_PERF_N], bool restart) {
    for (int i = 0; i < SIM_PERF_N; i++) { out[i] = gSimMs[i]; if (restart) gSimMs[i] = 0; }
    const int n = gSimTicks;
    if (restart) gSimTicks = 0;
    return n;
}

static void TickReal(float dt) {
    if (W3dCinematicActive()) { TickCinematic(dt); return; }
    extern double W3dNowMs();
    double _t = W3dNowMs(), _t2;
    gSimTicks++;
    // FISICA del Core (velocidad + rebotes): PRIMERO se integran las posiciones y despues
    // corren los scripts, asi el script ve la posicion nueva y resuelve el choque (rebotar)
    // en el MISMO frame. Es el mismo orden y el mismo dt que el runtime compilado
    // (W3dGameUpdate en game/w3drun.cpp) -> el juego se comporta igual en los dos lados.
    // LA BASE EN PAUSA (pauseScene con su nombre: un menu de pausa cargado encima): el mundo no avanza (su fisica,
    // sus hitbox, sus animaciones; sus scripts los frena W3dSceneIsFromActive). Igual que el runtime compilado
    const bool world = !W3dSceneBasePaused();
    if (world) W3dPhysicsStep(dt);
    // CUERPOS RIGIDOS (cajas con masa): mismo orden que la fisica minima, y los
    // scripts leen contacts() de ESTE paso en el mismo frame.
    if (world) W3dRigidStep(dt);
    _t2 = W3dNowMs(); gSimMs[SIM_PERF_PHYSICS] += _t2 - _t; _t = _t2;
    // HITBOX: solapes de ESTE paso (despues de mover los cuerpos) -> alEntrar / alQuedarse /
    // alSalir, y los contactos nuevos de los rigidos -> alTocar. Antes de los update():
    // el script ve el evento y su update() del mismo tick ya puede reaccionar. Mismo lugar
    // que en el runtime compilado (W3dGameUpdate).
    if (world) W3dHitboxStep(dt);
    _t2 = W3dNowMs(); gSimMs[SIM_PERF_HITBOX] += _t2 - _t; _t = _t2;
    // CAMARAS CON RIEL: el indice fraccionario (railIndex, el 5to valor de getRail)
    // lo horneaba SOLO el dibujo (Viewport3D::UpdateViewOrbit -> UpdatePosition), asi
    // que en ticks sin render -- harness `simplay`, o el primer tick antes del primer
    // frame -- los scripts leian el indice VIEJO y una VisZone modo curva recortaba
    // con la celda de OTRO nodo (cunas negras en el material de referencia). Se hornea
    // TAMBIEN aca, antes de correr los scripts: getRail() queda fresco en el tick, con
    // el mismo valor que dejaria un frame dibujado entre tick y tick (UpdatePosition
    // es deterministica e idempotente: el dibujo posterior recalcula lo mismo).
    { extern void W3dCamerasRailTick(); W3dCamerasRailTick(); }
    EntryPad();
    double _lua0 = W3dNowMs();
    int _act = 0;
    if (gLuaMsAccum.size() != gScripted.size()) gLuaMsAccum.assign(gScripted.size(), 0.0);
    for (size_t i = 0; i < gScripted.size(); i++) {
        // solo corren los scripts de la escena ACTIVA (y visibles): un objeto invisible o
        // de una escena inactiva no ejecuta su logica. Sin multi-escena EsDeActiva cae a
        // la regla vieja (solo visible), asi que el 3D/una-sola-UI se comporta igual.
        if (!W3dSceneIsFromActive(gScripted[i])) continue;
        double _s0 = W3dNowMs();
        W3dScriptUpdate(gScripted[i], dt);
        gLuaMsAccum[i] += W3dNowMs() - _s0;   // a quien se le va el tiempo (SimLuaPerfTop)
        _act++;                               // objeto con script que EFECTIVAMENTE corrio (isVisible/escena activa)
    }
    g_luaActiveScripts = _act;              // el "solo calcular las cosas cercanas": los no-visibles no cuentan
    g_luaScriptsTotal  = (int)gScripted.size();
    g_luaTickMs        = W3dNowMs() - _lua0;
    gSimMs[SIM_PERF_LUA] += g_luaTickMs;
    _t = W3dNowMs();
    // ANIMACION DE ESCENA pedida por los scripts (animScene de lua): avanzar el
    // reloj y APLICAR las curvas. Va DESPUES del bucle de scripts para que un
    // animScene() recien pedido pose los objetos en ESTE mismo frame (el editor
    // solo evalua estas curvas con kind 0; en el juego el dueno es este tick).
    if (world) W3dAnimSceneTick(dt);
    // CLIPS DE OBJETO (animObject de lua): cada objeto con su cabezal, varios a la vez. Mismo lugar
    // que la animacion de escena (en el runtime tambien).
    if (world) W3dAnimObjectsTick(dt);
    // ESQUELETOS: cada armature avanza su clip con su propio reloj (animClip de lua). Despues de los scripts,
    // igual que la animacion de escena: el clip recien pedido arranca en ESTE frame. (Mismo lugar en el runtime.)
    if (world) W3dArmaturesGameTick(dt);
    // SNAPSHOT del estado apretado para el flanco de pressed() del PROXIMO frame (DESPUES de los scripts,
    // igual que el runtime en W3dGameUpdate): asi pressed() funciona identico en el Play y compilado.
    BindsGameSnapshotPointers();
    _t2 = W3dNowMs(); gSimMs[SIM_PERF_ANIM] += _t2 - _t; _t = _t2;
    // STREAMING (io/Streaming.h): cargar lo que se acerco y descargar lo que se alejo, fuera de los update() y ANTES
    // del fin de frame (lo que genera arranca su update() desde el proximo tick). Mismo lugar que en el runtime
    W3dStreamingTick();
    gSimMs[SIM_PERF_STREAM] += W3dNowMs() - _t;
    W3dScriptEndFrame();   // idem para keyPressed()/buttonPressed(): UNA vez, despues de TODOS los objetos
                           // (y los destroy() del frame: ver SimObjectDestroy)
    // lo que instantiate() creo en este tick entra a la lista (su start() ya corrio): su update(), desde el proximo
    if (!gScriptedNew.empty()) { gScripted.insert(gScripted.end(), gScriptedNew.begin(), gScriptedNew.end()); gScriptedNew.clear(); }
    // el cambio de escena pedido por changeScene() se aplica ACA, al final del frame (no
    // en el callback: no mutar la iteracion de arriba). El init perezoso corre en este punto.
    W3dSceneApplyPending();
    // VERTEX ANIMS AL AVANZAR FRAME A FRAME EN PAUSA. Jugando, quien las hace
    // correr es el loop de render (main.cpp llama a UpdateAnimations una vez por
    // frame); pero con el timeline EN PAUSA ese loop las congela a proposito
    // ("el mundo del juego no respira si el juego no corre"). Sin esto, el boton
    // ">" del timeline simulaba un frame nuevo -- fisica, scripts, snapshot --
    // con el personaje congelado en la misma pose, y ESA pose quedaba grabada. Solo en
    // pausa, asi que jugando nunca se avanza dos veces.
    if (!PlayAnimation) UpdateAnimations(dt);
    gTick++;
    if (AnimIsGame || StartFrame + gTick <= EndFrame) CurrentFrame = StartFrame + gTick;
    g_redraw = true;
}

void SimTickPlay(float dt) {
    if (w3dEngine::W3dAudioPaused()) w3dEngine::W3dAudioPause(false);   // Play / reanudar: el mixer sigue donde estaba
    if (!gActive) { SimPlay(); if (!gActive) return; }
    { extern void FeedFingersGame(); FeedFingersGame(); } // multi-touch: dedos -> finger(n)
    TickReal(dt);
}

// agregar/quitar/cambiar un script CON el juego andando: sus instancias se recargan
// ya mismo (las variables internas de ESOS scripts arrancan de cero)
void SimScriptsChanged(Object* o) {
    if (!gActive || !o) return;
    W3dScriptUnloadFrom(o);
    bool has = (o->scriptData && !o->scriptData->scripts.empty());
    if (has) {
        W3dScriptLoad(o);
        SimReresolve(o);
        W3dScriptStart(o);
    }
    // si es su PRIMER script en vivo, entra a la lista de ejecucion
    bool inList = false;
    for (size_t i = 0; i < gScripted.size(); i++) if (gScripted[i] == o) inList = true;
    if (has && !inList) gScripted.push_back(o);
}

// jugar con el DEDO: el toque de un viewport se pasa al lienzo del juego (px,
// centro 0,0 como getPosPx) y llega a los scripts via tap()
// mapea un punto de pantalla (mx,my) del viewport 'v' al lienzo del juego (px, centro 0,0)
static void MapToGame(ViewportBase* v, int mx, int my, float* cx, float* cy) {
    // (P5) el override que este mapeo necesite es DE ESTE TOQUE, no del juego: al salir
    // se repone lo que habia (el del viewport ACTIVO). Sin esto, un click en el segundo
    // viewport dejaba SU pantalla puesta y los binds del juego (screen()) la leian.
    UI2D_OverrideWindowScope range;
    // VISTA DE JUEGO: re-fijar el override del lienzo de ESTE viewport ANTES de leer su tamano,
    // asi el input usa el MISMO lienzo que el dibujo (coherencia con varios viewports 2D)
    Editor2DFeedOverride(v);
    // idem para el 3D en modo juego con UI dinamica: su HUD se dibujo con el lienzo
    // overrideado a su pantalla (ver Viewport3D::RenderUI) y el input tiene que verlo
    if (v && v->ViewportKind() == 1) {
        Viewport3D* e3o = (Viewport3D*)v;
        if (e3o->hudOverride && e3o->hudW > 0.5f) UI2D_OverrideWindow(e3o, e3o->hudW, e3o->hudH);
    }
    float lw, lh; UI2D_CanvasSize(&lw, &lh);
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
void SimTouchScreen(ViewportBase* v, int mx, int my, bool active) {
    float cx, cy; MapToGame(v, mx, my, &cx, &cy);
    W3dScriptTouch(cx, cy, active);   // dedo 0 (compat: mouse / 1 dedo)
    BindsGamePoint(0, cx, cy, active);   // cache en LIENZO para pressed()/etc (igual que el runtime)
}
// MULTI-TOUCH: alimenta el dedo 'i' (0-based) al juego -> finger(i+1) en lua (2 jugadores tactiles)
void SimTouchScreenFinger(ViewportBase* v, int i, int mx, int my, bool active) {
    float cx, cy; MapToGame(v, mx, my, &cx, &cy);
    W3dScriptFinger(i, cx, cy, active);
    BindsGamePoint(i, cx, cy, active);
}
// MOUSE-OVER: posicion del mouse (sin clic) al juego -> mouse() en lua (resaltar la opcion bajo el cursor)
void SimMouseScreen(ViewportBase* v, int mx, int my, bool inside) {
    float cx, cy; MapToGame(v, mx, my, &cx, &cy);
    W3dScriptMouse(cx, cy, inside);
    // cache en LIENZO para el overlay (borde de hover de los botones), igual que el runtime
    BindsGameMouse(cx, cy, inside);
}

// paso a paso con la partida en PAUSA: hacia adelante simula un frame; hacia atras no hay nada (el juego no
// graba su pasado: no se rebobina)
bool SimStep(int dir) {
    if (!gActive) return false;
    if (dir > 0) TickReal(1.0f / 30.0f);
    return true;
}

void SimStop() {
    // el "Parar" del editor: nada sigue sonando (la musica secuenciada, los loops del motor...)
    w3dEngine::W3dAudioPause(false);
    w3dEngine::W3dSoundStopAll();
    if (!gActive) return;
    // una CINEMATICA sonando: la escena vuelve a como estaba y el juego a ser la raiz activa (sin su
    // alTerminar: la partida se corta)
    W3dCinematicAbort();
    W3dRootsSetCinematic(NULL);
    W3dRootsSetChangeGame(NULL);
    // las OTRAS escenas 3D por las que paso la partida vuelven a como las dejo el usuario (cada una
    // con SU contexto: el snapshot tambien repone sus capas del Mix) y se vuelve a la del Play
    if (!gBasesRoots.empty() || (gRootPlay && SceneCollection != gRootPlay)) {
        W3dSceneClear();
        for (size_t i = 0; i < gBasesRoots.size(); i++) {
            const int k = W3dObjectRoot(gBasesRoots[i].root);
            if (k >= 0 && W3dRootUse(k)) Apply(gBasesRoots[i].snap);
        }
        const int k = W3dObjectRoot(gRootPlay);
        if (k >= 0) W3dRootChangeActive(k);
        { extern void PropsForgetScene(); PropsForgetScene(); }
    }
    gBasesRoots.clear();
    gRootPlay = NULL;
    // lo que los scripts crearon jugando se va y lo que destruyeron vuelve (antes de reponer la foto: la foto
    // nombra a lo destruido, que tiene que estar de vuelta en su lugar)
    W3dScriptSetNewObject(NULL);
    W3dScriptSetObjectDestroy(NULL);
    W3dScriptSetObjectFree(NULL);
    SimUndoCreatedAndDestroyed();
    Apply(gBaseEditor);   // la escena del usuario, NO el frame 0 post-start()
    ScriptsRestore();     // ...y lo de sus scripts (lo que se cambio en el inspector jugando)
    // STREAMING: lo que cargo jugando ya se libero (era creado en el Play); las diferidas que el Play solto se vuelven a
    // generar como estaban y se sueltan los pedidos de la partida
    W3dStreamingSessionEnd();
    W3dMixScenesBasesRestore();   // las bases del mix, como antes del Play (no mueve nada)
    W3dScriptUnloadAll();
    W3dScriptReleaseKeys();
    W3dAnimSceneReset();    // que no quede una animScene() sonando para la proxima partida
    W3dAnimObjectsReset();  // ...ni un clip de objeto (animObject)
    W3dRigidClear();        // los cuerpos rigidos runtime mueren con la partida
    W3dHitboxClear();       // y los pares de hitbox / contactos (sin eventos)
    gArrow[0] = gArrow[1] = gArrow[2] = gArrow[3] = false;  // flechas del teclado
    BindsGameResetPointers();   // soltar el finger/mouse: que no quede un toque viejo para la proxima partida
    W3dSceneClear();            // soltar el mapa de escenas / activa (el editor vuelve a elegir UI por ActiveObj)
    W3dSceneLayersClear();      // ...y las capas (loadScene): sin pausa, sin mensajes, las 2D visibles otra vez
    gScripted.clear();
    gTick = 0;
    gActive = false;
    CurrentFrame = StartFrame;
    if (gBaseEyeRoot && W3dObjectRoot(gBaseEyeRoot) >= 0) gBaseEyeRoot->visible = gBaseEye;   // (el ojo del editor)
    gBaseEyeRoot = NULL;
    SimReturnToEdited();
    g_redraw = true;
}

// ---- teclado del juego -----------------------------------------------------
void SimKeySDL(int sdlk, bool down) {
#ifdef W3D_SYMBIAN
    (void)sdlk; (void)down;   // en Symbian el teclado del juego entra por otra via (keypad propio)
#else
    const char* n  = NULL;
    char letter[2] = { 0, 0 };
    // LAS FLECHAS DEL TECLADO tambien mueven el stick izquierdo (ver gArrow arriba):
    // sin esto un juego que solo lee stick("izq") -- lo normal -- no se podia jugar
    // con el teclado, y cada script tenia que reimplementar el cruce arrows->axes.
    if      (sdlk == SDLK_UP)    gArrow[0] = down;
    else if (sdlk == SDLK_DOWN)  gArrow[1] = down;
    else if (sdlk == SDLK_LEFT)  gArrow[2] = down;
    else if (sdlk == SDLK_RIGHT) gArrow[3] = down;
    if (sdlk >= SDLK_a && sdlk <= SDLK_z) { letter[0] = (char)('a' + (sdlk - SDLK_a)); n = letter; }
    else if (sdlk == SDLK_UP)     n = "up";
    else if (sdlk == SDLK_DOWN)   n = "down";
    else if (sdlk == SDLK_LEFT)   n = "left";
    else if (sdlk == SDLK_RIGHT)  n = "right";
    else if (sdlk == SDLK_SPACE)  n = "space";
    else if (sdlk == SDLK_LSHIFT || sdlk == SDLK_RSHIFT) n = "shift";   // correr, agacharse...
    else if (sdlk == SDLK_LCTRL  || sdlk == SDLK_RCTRL)  n = "ctrl";
    else if (sdlk == SDLK_RETURN || sdlk == SDLK_KP_ENTER) n = "enter";
    else if (sdlk >= SDLK_0 && sdlk <= SDLK_9) { letter[0] = (char)('0' + (sdlk - SDLK_0)); n = letter; }
    if (n) W3dScriptKey(n, down);
#endif
}


// ============================================================================
//  EL DESHACER DE LO QUE HACE UN SCRIPT FUERA DEL JUEGO (2026-11-11, pedido del dueno: "si un script hace cambios en la
//  escena, cada cosa que hace se tiene que poder deshacer; donde no hay historial es durante el gameplay, porque al
//  detener se reinicia todo y seria gastar memoria y rendimiento"). Hoy: un BOTON de las propiedades tocado con el juego
//  parado (un complemento del editor). Se graba asi:
//    Iniciar:   la FOTO de la escena (la misma de Parar: posiciones, rotaciones, escalas, visibilidad, colores, hitbox,
//               texto...), los objetos que hay, y el gancho de las MALLAS (antes de que lua toque los vertices, UV o
//               colores de una, su geometria: UndoCaptureMeshGeo, una vez por malla).
//    Confirmar: lo que el script CREO (instanciar) es una creacion como la de Shift+D (Ctrl+Z la saca); lo que pidio
//               DESTRUIR se borra como con la tecla Supr (detachado, Ctrl+Z lo devuelve: antes quedaba en la cola
//               hasta el proximo Play); y si la foto cambio, un paso que la repone. Todo es UN paso del historial.
//  Jugando no se graba nada.
// ============================================================================
struct SimUndoScript { SimSnap other; };
static void SimUndoScriptApply(void* d) {
    SimUndoScript* u = (SimUndoScript*)d;
    SimSnap now; Snapshot(&now);
    Apply(u->other);
    u->other = now;   // (el mismo paso sirve para ir y volver)
    g_redraw = true;
}
static void SimUndoScriptUnlink(void* d, Object* deleted) {
    SimUndoScript* u = (SimUndoScript*)d;
    std::set<Object*> outside; outside.insert(deleted);
    PruneSnap(u->other, outside);
}
static void SimUndoScriptFree(void* d) { delete (SimUndoScript*)d; }
// cambio algo de lo que esta en la foto? (si no, no se agrega un paso vacio al historial: un boton que solo imprime)
static bool LayersEqual(const std::vector<W3dLayerAnim>& a, const std::vector<W3dLayerAnim>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        const W3dLayerAnim& x = a[i]; const W3dLayerAnim& y = b[i];
        if (x.anim != y.anim || x.influence != y.influence || x.mode != y.mode || x.visible != y.visible ||
            x.bone != y.bone || x.from != y.from || x.vel != y.vel || x.loop != y.loop || x.gameFrame != y.gameFrame) return false;
    }
    return true;
}
static bool SnapshotsEqual(const SimSnap& a, const SimSnap& b) {
    if (a.ents.size() != b.ents.size() || a.arms.size() != b.arms.size() || a.hier.size() != b.hier.size()) return false;
    for (size_t i = 0; i < a.ents.size(); i++) {
        const SimEnt& x = a.ents[i]; const SimEnt& y = b.ents[i];
        if (x.o != y.o || x.px != y.px || x.py != y.py || x.pz != y.pz || x.vis != y.vis || x.renderiz != y.renderiz) return false;
        if (x.rotEuler.x != y.rotEuler.x || x.rotEuler.y != y.rotEuler.y || x.rotEuler.z != y.rotEuler.z) return false;
        if (x.rot.x != y.rot.x || x.rot.y != y.rot.y || x.rot.z != y.rot.z || x.rot.w != y.rot.w) return false;
        if (x.scale.x != y.scale.x || x.scale.y != y.scale.y || x.scale.z != y.scale.z) return false;
        if (x.isLight) for (int k = 0; k < 4; k++) if (x.dif[k] != y.dif[k] || x.amb[k] != y.amb[k] || x.spe[k] != y.spe[k]) return false;
        if (x.isPart && x.partActive != y.partActive) return false;
        if (x.isFog) for (int k = 0; k < 6; k++) if (x.fog[k] != y.fog[k]) return false;
        if (x.isCam && (x.rail != y.rail || x.offRail != y.offRail || x.railNode != y.railNode || x.lookRail != y.lookRail)) return false;
        if (x.hasVel && (x.vx != y.vx || x.vy != y.vy || x.vz != y.vz)) return false;
        if (x.hasVA && (x.vaPlay != y.vaPlay || x.vaAnim != y.vaAnim || x.vaNext != y.vaNext)) return false;
        if (x.isHb && (x.hbActive != y.hbActive || x.hbSize[0] != y.hbSize[0] || x.hbSize[1] != y.hbSize[1] || x.hbSize[2] != y.hbSize[2])) return false;
        if (x.es2d && (x.rot2d != y.rot2d || x.width != y.width || x.height != y.height || x.opacity != y.opacity ||
                       x.scrollX != y.scrollX || x.scrollY != y.scrollY)) return false;
        if (x.isText && x.text != y.text) return false;
    }
    for (size_t i = 0; i < a.arms.size(); i++) {
        const SimArm& x = a.arms[i]; const SimArm& y = b.arms[i];
        if (x.a != y.a || x.clip != y.clip || x.frame != y.frame || x.vel != y.vel || x.loop != y.loop ||
            x.retarget != y.retarget || !LayersEqual(x.layers, y.layers)) return false;
    }
    for (size_t i = 0; i < a.hier.size(); i++)
        if (a.hier[i].o != b.hier[i].o || a.hier[i].retarget != b.hier[i].retarget || !LayersEqual(a.hier[i].layers, b.hier[i].layers)) return false;
    return LayersEqual(a.mixScenes, b.mixScenes);
}
static SimSnap* gUndoScriptPrev = NULL;
static std::set<Object*> gUndoScriptMeshes;      // las mallas ya guardadas en esta grabacion (una vez cada una)
static std::set<unsigned> gUndoScriptHad;       // los objetos (por serial) que habia antes del script
static void SerialsOf(Object* o, std::set<unsigned>& out) {
    if (!o) return;
    out.insert(o->serial);
    for (size_t i = 0; i < o->Childrens.size(); i++) SerialsOf(o->Childrens[i], out);
}
// las RAICES de lo nuevo: un objeto que no estaba colgado de uno que si (sus hijos son parte de el)
static void NewRoots(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (!gUndoScriptHad.count(h->serial)) out.push_back(h);
        else NewRoots(h, out);
    }
}
static void SimUndoScriptBeforeTouchMesh(Object* o) {
    if (!gUndoScriptPrev || !o || o->getType() != ObjectType::mesh || gUndoScriptMeshes.count(o)) return;
    gUndoScriptMeshes.insert(o);
    UndoCaptureMeshGeo((Mesh*)o);
}
void SimUndoScriptStart() {
    if (SimActive() || gUndoScriptPrev) return;   // jugando no hay historial (y no se anida)
    UndoGroupStart();
    gUndoScriptPrev = new SimSnap();
    Snapshot(gUndoScriptPrev);
    gUndoScriptMeshes.clear();
    gUndoScriptHad.clear();
    SerialsOf(SceneCollection, gUndoScriptHad);
    W3dScriptBeforeTouchMeshHook = SimUndoScriptBeforeTouchMesh;
}
void SimUndoScriptConfirm() {
    if (!gUndoScriptPrev) return;
    W3dScriptBeforeTouchMeshHook = NULL;
    // lo CREADO y lo que pidio DESTRUIR: por las mismas puertas que el editor (Shift+D / Supr), que trabajan con lo
    // elegido. Al final queda elegido lo creado (como despues de Shift+D); si no creo nada, lo que estaba (sin lo borrado)
    std::vector<Object*> created; NewRoots(SceneCollection, created);
    std::vector<Object*> deleteValue;  W3dScriptTakeDestroyRequests(deleteValue);
    if ((!created.empty() || !deleteValue.empty()) && state == editNavigation && InteractionMode == ObjectMode) {
        const std::vector<Object*> selPrev = ObjSelects;
        Object* actPrev = ActiveObj;
        std::set<Object*> deleted(deleteValue.begin(), deleteValue.end());
        if (!created.empty()) {
            DeselectAll();
            for (size_t i = 0; i < created.size(); i++) created[i]->Select();
            UndoCaptureCreation();      // (nace "en escena": Ctrl+Z los saca, Ctrl+Y los devuelve)
        }
        if (!deleteValue.empty()) {
            DeselectAll();
            for (size_t i = 0; i < deleteValue.size(); i++) deleteValue[i]->Select();
            UndoCaptureDeleted(false);  // los DETACHA (sin liberar): Ctrl+Z los devuelve
        }
        DeselectAll();
        if (!created.empty()) {
            for (size_t i = 0; i < created.size(); i++) if (!deleted.count(created[i])) created[i]->Select();
        } else {
            for (size_t i = 0; i < selPrev.size(); i++) if (!deleted.count(selPrev[i])) selPrev[i]->Select();
            if (actPrev && !deleted.count(actPrev) && actPrev->select) ActiveObj = actPrev;
        }
    } else if (!deleteValue.empty()) {
        // (fuera del Modo Objeto no se borra: que el pedido no quede colgado hasta el proximo Play)
        w3dLogf("[script] destruir() con el juego parado fuera del Modo Objeto: no se borro nada");
    }
    SimSnap after; Snapshot(&after);
    if (!SnapshotsEqual(*gUndoScriptPrev, after)) {
        SimUndoScript* u = new SimUndoScript();
        u->other = *gUndoScriptPrev;
        // (lo que se borro recien queda en la foto: esta detachado, vivo mientras su paso este en el historial)
        UndoExternal f; f.apply = SimUndoScriptApply; f.unlink = SimUndoScriptUnlink; f.free = SimUndoScriptFree;
        UndoPushExternal(f, u);
    }
    delete gUndoScriptPrev; gUndoScriptPrev = NULL;
    gUndoScriptMeshes.clear();
    gUndoScriptHad.clear();
    // SECUENCIAL: Ctrl+Z deshace al reves (la foto, lo borrado, lo creado, las mallas); Ctrl+Y al derecho
    UndoGroupSequentialEnd();
}
