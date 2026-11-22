// ============================================================================
//  W3dTestsIntegration.cpp — la prueba CRUZADA de tres areas que se hicieron por
//  separado: las MALLAS como recurso compartido (objects/MeshResource.h), los
//  ANIMSETS (animation/W3dAnimSet.h) y el HITBOX (objects/Hitbox.h). Cada area
//  tiene sus pruebas; aca se prueba que conviven en la MISMA escena, que es el caso
//  de uso: un personaje (esqueleto + cuerpo skinneado + sensor) repetido muchas
//  veces compartiendo memoria.
//
//  Comandos:
//    personajesmin <carpeta> <n>
//        el juego 3D minimo (juego3dmin) + n PERSONAJES en fila (x = 4k, z = -3). El PRIMERO
//        se arma por codigo:
//          - un armature "Personaje" (2 huesos) con DOS clips ("Saludo" y "Quieto");
//          - un cuerpo "Cuerpo" (cilindro skinneado a su armature), que pasa a ser el
//            recurso de malla "Cuerpo";
//          - un hitbox "Sensor" HIJO del armature (Add > Hitbox con el armature activo:
//            del tamano del cuerpo), filtro "jugador". El armature cuenta alEntrar/
//            alQuedarse/alSalir en getShared("p<k>_...").
//        Los DEMAS son el personaje entero duplicado con Alt+D (esqueleto + cuerpo + sensor
//        elegidos, NewInstance: la misma puerta que el teclado): cada copia es otro esqueleto
//        con los MISMOS clips en memoria (un animset), otro cuerpo con la MISMA malla
//        skinneado a SU esqueleto y otro sensor hijo. Se ubica en la fila, alterna el clip
//        activo (par = Saludo, impar = Quieto: poses distintas con los mismos clips) y cambia
//        el prefijo de su contador.
//        Un "Jugador" (vacio con un hitbox hijo etiqueta "jugador") pasa por delante de
//        todos a paso FIJO por tick y cuenta lo suyo en getShared("jugador_..."), y un
//        "Juez" deja el veredicto en getShared("juez") y en el log ("[personajes] OK ...")
//        y pide quit(): el mismo juez en el Play del editor y en el juego compilado.
//        Tambien exige que animLength vea los clips de TODOS los personajes (en el juego,
//        que el .w3da haya llegado) y que los esqueletos se MUEVAN: la punta del brazo del
//        primero cambia entre el arranque y un cabezal avanzado (getShared("pose") y
//        "pose=mueve" en el log; un juego que deja los esqueletos en bind da "pose=quieta").
//    personajesver <n> [compartido 0|1] [frame F]
//        la escena de personajesmin en memoria: todos los cuerpos usan EL MISMO recurso y
//        su geometria de reposo es la MISMA memoria; con 'compartido 1' todos los armatures
//        usan EL MISMO animset (mismo recurso, mismos clips en memoria). Poses POR
//        INSTANCIA en el cuadro F (default 10): mismo clip activo = skinning identico bit a
//        bit, clip distinto = pose distinta, cada uno en su propio buffer.
//    personajeslog <carpeta> <nombre> <esperado> [<esperado2>...]
//        corre el juego compilado (modo debug: deja su whisk3d.log) y exige el veredicto
//        OK del juez con cada 'esperado' en su linea (ej. "p=1/5/1" "pose=mueve"): lo mismo
//        que el Play.
//    dupjerarquia vinculado|copia <objeto>... [+hijos]
//        elige esos objetos (con '+hijos', cada uno con TODO su subarbol, como elegirlos a mano
//        en el outliner) y los duplica por la MISMA puerta que el teclado: Alt+D (NewInstance) o
//        Shift+D (DuplicatedObject). Informa las copias que quedaron elegidas (las RAIZ).
//    clipactivo <armature> <clip>
//        elige el clip activo del armature por nombre (la lista de clips de la pestania
//        Animation): 'charactersshow' compara las poses de los que tienen el mismo clip activo.
//    objrel <objeto> [nada] [padre P|-] [skin A|-] [hijos N] [tipo T]
//        el lugar de un objeto en el arbol: 'nada' = no esta en la escena; 'padre -' = cuelga
//        de la raiz; 'skin' = el esqueleto que lo deforma (Mesh::skinArmature Y el target de su
//        modificador Armature, que tienen que coincidir); 'children' = cuantos hijos directos;
//        'tipo' = armature | mesh | hitbox | instance | empty.
//    personajesdirector <carpeta>
//        agrega a la escena de personajesmin (n >= 2) un "Director" cuyo start() cambia
//        EN JUEGO el estado de animacion de los esqueletos con la API lua: "Personaje" pasa
//        a "Quieto" sin loop y a velocidad 2; "Personaje.001" arranca una transicion de 6
//        frames y una capa del mix ("Saludo" sumado al 50%); y se crea una capa del mix
//        de ESCENAS con la animacion de escena "Paseo" (un "Paseante" que va de x=-20 a
//        x=-10), despues de subir al Paseante a y=50: la base que el mix anota jugando NO
//        es la del editor. Es lo que el Stop tiene que devolver como lo dejo el usuario.
//    esqjuego <armature> [clip C|-] [frame F] [vel V] [loop 0|1] [termino 0|1] [capas N] [trans 0|1]
//    esqjuego escenas [capas N]
//        el estado de JUEGO de un esqueleto (clip activo, cabezal, velocidad, loop, fin del
//        clip, capas del mix, transicion en curso) o la cantidad de capas del mix de
//        escenas. Siempre informa; cada par que se pase se exige (el cabezal con tol 0.01).
//    mixescena mezclar <capa> <frame> <objeto>
//    mixescena soltar <objeto> <x> <y> <z>
//        el MIX DE ESCENAS del editor: 'mezclar' agrega la capa (al 100%) y la aplica en ese
//        frame (el objeto se tiene que mover: la capa lo toca); 'soltar' sale del Mix
//        (W3dMixScenesRelease), saca las capas y exige que el objeto quede en (x,y,z): la
//        BASE que el mix anoto, o sea donde estaba antes de mezclarlo (tol 0.001).
//    reporaiz <esperado> [ajustes <ruta>|-] [entorno <ruta>|-] [via <camino>]
//        la raiz del repo que usa "Compilar juego" (CompileGameRepoRoot) tiene que ser la
//        carpeta 'esperado' (se comparan las rutas reales). 'settings' y 'entorno' ponen, SOLO
//        durante el comando, cfg.repoPath y la variable W3D_REPO ('-' = vacias): una ruta que
//        no es el repo se tiene que saltear. 'via' exige el camino que la dio (ajustes,
//        entorno, res, binario o fuentes).
// ============================================================================
#include "test/W3dTestsIntegration.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Armature.h"
#include "objects/Empty.h"
#include "objects/Hitbox.h"
#include "objects/ObjectMode.h"        // NewInstance (Alt+D)
#include "io/ProjectMeshes.h"          // W3dMeshCreateResource
#include "io/W3dResources.h"
#include "animation/SkeletalAnimation.h"
#include "animation/Animation.h"       // ActiveAnimKind (las poses como en el juego)
#include "animation/W3dAnimSet.h"      // W3dArmatureAnimSetResource
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"          // WeightPaintEnsureMap
#include "script/W3dScript.h"
#include "base/W3dInteractionState.h"  // Alt+D exige Modo Objeto navegando
#include "io/CompileGame.h"        // CompileGameRepoRoot (reporaiz)
#include "variables.h"                // cfg.repoPath (reporaiz)
#include "w3dFilesystem.h"
#include <math.h>                     // fabsf (esqjuego)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

extern Object* FindObjectByName(Object* node, const std::string& name);

namespace {

// ---------------------------------------------------------------------------
//  los .lua del juego de prueba
// ---------------------------------------------------------------------------
// el contador de eventos de hitbox en getShared("<prefijo>_...")
const char* kLuaCounter =
    "-- contador de eventos de hitbox (prueba cruzada del motor)\n"
    "properties = { prefijo = \"x\" }\n"
    "local p = \"x\"\n"
    "local function sum(c) local k = p .. \"_\" .. c; setShared(k, (getShared(k) or 0) + 1) end\n"
    "function start()\n"
    "  p = property(\"prefijo\")\n"
    "  setShared(p .. \"_entrar\", 0); setShared(p .. \"_quedarse\", 0); setShared(p .. \"_salir\", 0)\n"
    "end\n"
    "function onEnter(other, mine, own) sum(\"entrar\"); if other then setShared(p .. \"_otro\", getName(other)) end end\n"
    "function onStay(other, mine, own) sum(\"quedarse\") end\n"
    "function onExit(other, mine, own) sum(\"salir\") end\n";
// avanza en X un paso FIJO por tick (determinista aunque el dt del juego varie). Las propiedades "y" y "z" son las de
// cuando lua era Y arriba ("y" la ALTURA, "z" la profundidad; asi vienen en las refs): con Z arriba van cruzadas
const char* kLuaMove =
    "-- avanza en X un paso fijo por tick (x = x0 + paso * n)\n"
    "properties = { x0 = 0, paso = 0.4, y = 0, z = 0 }\n"
    "local n = 0\n"
    "function update(dt)\n"
    "  n = n + 1\n"
    "  setPosition(self(), property(\"x0\") + property(\"paso\") * n, property(\"z\"), property(\"y\"))\n"
    "end\n";
// EL JUEZ: cuando el jugador paso a todos, mira los contadores y los clips de cada personaje, y
// que los esqueletos se MUEVAN: la punta del brazo del primero (clip Saludo) al arrancar contra la
// de un cabezal avanzado (frame >= 8). Un juego que no reproduce los clips (esqueletos en bind)
// deja las dos iguales: "pose=quieta" y FALTA.
const char* kLuaJudge =
    "-- veredicto de la prueba cruzada (malla + animset compartidos + hitbox)\n"
    "properties = { n = 1, fin = 0, largo = 0, largo2 = 0 }\n"
    "local ready = false\n"
    "local ticks = 0\n"
    "local x1, y1, z1\n"
    "local function c(k) return getShared(k) or -1 end\n"
    "local function name(k) if k == 0 then return \"Personaje\" end return string.format(\"Personaje.%03d\", k) end\n"
    "function update(dt)\n"
    "  if ready then return end\n"
    "  ticks = ticks + 1\n"
    "  local a0 = find(\"Personaje\")\n"
    "  if ticks == 1 then x1, y1, z1 = bonePoint(a0, \"brazo\", 0, 0, 1) end   -- el (0, 1, 0) del hueso en el motor: Z arriba\n"
    "  local x = getPosition(find(\"Jugador\"))\n"
    "  local f = animFrame(a0) or 0\n"
    "  if (x < property(\"fin\") or f < 8) and ticks < 100000 then return end\n"
    "  ready = true\n"
    "  local x2, y2, z2 = bonePoint(a0, \"brazo\", 0, 0, 1)\n"
    "  local d = -1\n"
    "  if x1 and x2 then d = math.abs(x2 - x1) + math.abs(y2 - y1) + math.abs(z2 - z1) end\n"
    "  local pose = (d > 0.01) and \"mueve\" or \"quieta\"\n"
    "  local n = property(\"n\")\n"
    "  local q0 = c(\"p0_quedarse\")\n"
    "  local ok = c(\"jugador_entrar\") == n and c(\"jugador_salir\") == n and q0 >= 1\n"
    "  local clips = 0\n"
    "  for k = 0, n - 1 do\n"
    "    local p = \"p\" .. k\n"
    "    if c(p .. \"_entrar\") ~= 1 or c(p .. \"_salir\") ~= 1 or c(p .. \"_quedarse\") ~= q0 or c(p .. \"_otro\") ~= \"Jugador\" then ok = false end\n"
    "    local a = find(name(k))\n"
    "    -- animLargo solo MIRA el clip (animClip lo pondria a reproducir y cambiaria las poses)\n"
    "    if a and animLength(a, \"Saludo\") == property(\"largo\") and animLength(a, \"Quieto\") == property(\"largo2\") then clips = clips + 1 end\n"
    "  end\n"
    "  if clips ~= n then ok = false end\n"
    "  if pose ~= \"mueve\" then ok = false end\n"
    "  local line = string.format(\"[personajes] %s n=%d p=%d/%d/%d jugador=%d/%d clips=%d pose=%s d=%.3f frame=%.1f ticks=%d\",\n"
    "    ok and \"OK\" or \"FALTA\", n, c(\"p0_entrar\"), q0, c(\"p0_salir\"), c(\"jugador_entrar\"), c(\"jugador_salir\"), clips, pose, d, f, ticks)\n"
    "  info(line)\n"
    "  setShared(\"pose\", pose)\n"
    "  setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "  quit()\n"
    "end\n";
// EL DIRECTOR: cambia en juego lo que la API lua de esqueletos puede cambiar (personajesdirector)
const char* kLuaDirector =
    "-- cambia EN JUEGO el estado de animacion de los esqueletos (prueba del Stop y del rewind)\n"
    "function start()\n"
    "  local a = find(\"Personaje\")\n"
    "  local b = find(\"Personaje.001\")\n"
    "  animClip(a, \"Quieto\", false)          -- otro clip, sin loop\n"
    "  animSpeed(a, 2)\n"
    "  animTransition(b, 6)                   -- 6 frames a 30 fps: 0.2 s fundiendo\n"
    "  animLayer(b, 1, \"Saludo\", 0.5, \"add\")\n"
    "  local p = find(\"Paseante\")\n"
    "  setPosition(p, -20, -8, 50)           -- el mix lo toca desde ACA (su base en la partida)\n"
    "  sceneLayer(1, \"Paseo\", 0.25)          -- una capa del mix de ESCENAS (global)\n"
    "end\n";

bool Write(const std::string& path, const char* text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fputs(text, f);
    fclose(f);
    return true;
}

void Script(Object* o, const char* path) {
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    W3dScriptEntry e; e.path = path;
    o->scriptData->scripts.push_back(e);
}
void Ref(Object* o, const std::string& prop, const std::string& value) {
    o->scriptData->scripts.back().refs.push_back(std::make_pair(prop, value));
}
std::string Num(float v) { char b[48]; snprintf(b, sizeof(b), "%g", v); return b; }

// un clip de un solo hueso: rotacion lineal de 'a' a 'b' grados en el eje 'comp'
SkeletalAnimation* Clip(const char* name, int bone, int comp, float a, float b, int end) {
    SkeletalAnimation* c = new SkeletalAnimation(name);
    c->FrameRate = 24; c->startFrame = 1; c->endFrame = end;
    BoneTrack tr; tr.bone = bone;
    AnimProperty ap; ap.Property = AnimRotation; ap.component = comp;
    keyFrame k0; k0.frame = 1;   k0.value = a; k0.Interpolation = KfLinear; ap.keyframes.push_back(k0);
    keyFrame k1; k1.frame = end; k1.value = b; k1.Interpolation = KfLinear; ap.keyframes.push_back(k1);
    tr.Properties.push_back(ap);
    c->tracks.push_back(tr);
    return c;
}

// el esqueleto de un personaje (2 huesos) con sus dos clips (IGUALES en todos)
const int kGreetingLength = 20;
const int kStillLength    = 30;
Armature* NewSkeleton(const Vector3& pos, int k) {
    Armature* a = new Armature(NULL, pos);
    a->SetNameObj("Personaje");
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0, 0, 0); r.tail = Vector3(0, 1, 0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0, 1, 0); c.tail = Vector3(0, 2, 0); a->bones.push_back(c); }
    PrepareAuthoredSkin(a);
    a->animations.push_back(Clip("Saludo", 1, AnimZ, 0.0f, 60.0f, kGreetingLength));
    a->animations.push_back(Clip("Quieto", 0, AnimY, 0.0f, 35.0f, kStillLength));
    a->activeAnim = k % 2;   // par = Saludo, impar = Quieto
    return a;
}

// el cuerpo del primer personaje: un cilindro con los pesos por altura
Mesh* NewBody(Armature* a) {
    Mesh* s = (Mesh*)NewMesh(MeshType::cylinder, a, false);
    if (!s) return NULL;
    s->SetNameObj("Cuerpo");
    s->pos = Vector3(0, 0, 0);
    s->GenerateRender();
    WeightPaintEnsureMap(s);
    VertexGroup* vr = new VertexGroup("raiz");
    VertexGroup* vb = new VertexGroup("brazo");
    std::vector<char> done(s->vertexSize, 0);
    for (int i = 0; i < s->vertexSize; i++) {
        const int cp = s->vertCtrlPoint[i];
        if (done[cp]) continue;
        done[cp] = 1;
        if (s->vertex[i * 3 + 1] > 0.0f) { vb->verts.push_back(cp); vb->weights.push_back(1.0f); }
        else { vr->verts.push_back(cp); vr->weights.push_back(1.0f); }
    }
    s->vertexGroups.push_back(vr);
    s->vertexGroups.push_back(vb);
    Modifier* md = new Modifier(ModifierType::Armature, ModifierTypeName(ModifierType::Armature));
    md->target = (Object*)a;
    s->modifiers.push_back(md);
    s->skinArmature = a;
    return s;
}

std::string CharacterName(int k) {
    if (k == 0) return "Personaje";
    char b[32]; snprintf(b, sizeof(b), "Personaje.%03d", k);
    return b;
}

template <class T> T* TypeChild(Object* o, ObjectType t) {
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (o->Childrens[i] && o->Childrens[i]->getType() == t) return (T*)o->Childrens[i];
    return NULL;
}

// ---------------------------------------------------------------------------
//  personajesmin
// ---------------------------------------------------------------------------
bool CmdCharactersMin(std::istringstream& ss, std::string& err) {
    std::string dir; int n = 0;
    ss >> dir >> n;
    if (dir.empty() || n < 1) { err = "charactersmin: uso: charactersmin <carpeta> <n>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "personajesmin: " + e2; return false; }
    if (!Write(dir + "/pj_contador.lua", kLuaCounter) || !Write(dir + "/pj_mover.lua", kLuaMove) ||
        !Write(dir + "/pj_juez.lua", kLuaJudge)) {
        err = "charactersmin: no pude escribir los .lua en " + dir; return false;
    }
    const float z = -3.0f;
    // EL PRIMER PERSONAJE, por codigo: esqueleto con sus clips, cuerpo como recurso "Cuerpo" y el
    // sensor (Add > Hitbox con el armature activo: hijo, del tamano de su cuerpo)
    Armature* a0 = NewSkeleton(Vector3(0.0f, 0.0f, z), 0);
    Mesh* body0 = NewBody(a0);
    if (!body0) { err = "charactersmin: NewMesh fallo"; return false; }
    if (!W3dMeshCreateResource(body0, "Cuerpo")) { err = "charactersmin: no pude crear el recurso 'Cuerpo'"; return false; }
    Hitbox* sensor0 = HitboxCreateFitted(a0);
    if (!sensor0) { err = "charactersmin: HitboxCrearAjustado fallo"; return false; }
    sensor0->SetNameObj("Sensor");
    sensor0->filter = "jugador";
    Script(a0, "pj_contador.lua"); Ref(a0, "prefijo", "p0");
    // LOS DEMAS son su DUPLICADO VINCULADO: Alt+D con el esqueleto, el cuerpo y el sensor elegidos,
    // por la MISMA puerta que el teclado (Modo Objeto, navegando). Cada copia es otro personaje:
    // otro esqueleto con los MISMOS clips (los del primero pasan a un animset en memoria), otro
    // cuerpo con la MISMA malla re-skinneado a SU esqueleto y otro sensor hijo. Despues se ubica en
    // la fila, alterna el clip activo (par = Saludo, impar = Quieto) y cambia el prefijo de su
    // contador (el script viene copiado con los valores del original, como en el editor).
    for (int k = 1; k < n; k++) {
        InteractionMode = ObjectMode;
        state = editNavigation;
        DeselectAll();
        a0->Select(); body0->Select(); sensor0->Select();
        ActiveObj = a0;
        NewInstance();
        state = editNavigation;   // Alt+D deja agarrado el MOVER (como el teclado): se suelta
        if (!ActiveObj || ActiveObj == a0 || ActiveObj->getType() != ObjectType::armature) {
            err = "charactersmin: Alt+D no creo otro esqueleto"; return false;
        }
        Armature* a = (Armature*)ActiveObj;
        if (ObjSelects.size() != 1) { err = "charactersmin: Alt+D dejo elegido algo mas que el esqueleto nuevo"; return false; }
        a->pos = Vector3(4.0f * (float)k, 0.0f, z);
        a->activeAnim = k % 2;
        char pref[32]; snprintf(pref, sizeof(pref), "p%d", k);
        if (!a->scriptData || a->scriptData->scripts.empty()) { err = "charactersmin: la copia no trajo su script"; return false; }
        std::vector<std::pair<std::string, std::string> >& refs = a->scriptData->scripts[0].refs;
        for (size_t r = 0; r < refs.size(); r++) if (refs[r].first == "prefijo") refs[r].second = pref;
    }
    // el jugador: pasa por delante de todos (los personajes estan cada 4 m; 4 / 0.4 = 10 ticks)
    const float x0 = -3.05f;
    Empty* player  = new Empty(NULL, Vector3(x0, 0.0f, z));
    player->SetNameObj("Jugador");
    Script(player, "pj_mover.lua");
    Ref(player, "x0", Num(x0)); Ref(player, "z", Num(z));
    Script(player, "pj_contador.lua"); Ref(player, "prefijo", "jugador");
    Hitbox* box = new Hitbox(player, Vector3(0, 0, 0));
    box->SetNameObj("CajaJugador");
    box->size[0] = box->size[1] = box->size[2] = 0.5f;
    box->label = "jugador";
    // el juez: veredicto cuando el jugador dejo atras al ultimo personaje
    Empty* judge = new Empty(NULL, Vector3(0.0f, -10.0f, 0.0f));
    judge->SetNameObj("Juez");
    Script(judge, "pj_juez.lua");
    { char b[16]; snprintf(b, sizeof(b), "%d", n); Ref(judge, "n", b); }
    Ref(judge, "fin", Num(4.0f * (float)(n - 1) + 3.0f));
    { char b[16]; snprintf(b, sizeof(b), "%d", kGreetingLength); Ref(judge, "largo", b); }
    { char b[16]; snprintf(b, sizeof(b), "%d", kStillLength); Ref(judge, "largo2", b); }
    DeselectAll(); ActiveObj = NULL;
    printf("      [personajesmin] juego3dmin + %d personajes (el primero por codigo, los demas su Alt+D entero: esqueleto + cuerpo + sensor) + jugador + juez en '%s'\n",
           n, dir.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  personajesver
// ---------------------------------------------------------------------------
bool CmdCharactersVer(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 2) { err = "charactersshow: uso: charactersshow <n> [compartido 0|1] [frame F] (n >= 2)"; return false; }
    int shared = -1, frame = 10;
    std::string k;
    while (ss >> k) {
        if (k == "shared") ss >> shared;
        else if (k == "frame") ss >> frame;
        else { err = "charactersshow: argumento desconocido '" + k + "'"; return false; }
    }
    std::vector<Armature*> arms;
    std::vector<Mesh*> bodies;
    for (int i = 0; i < n; i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, CharacterName(i)) : NULL;
        if (!o || o->getType() != ObjectType::armature) { err = "charactersshow: falta el armature '" + CharacterName(i) + "'"; return false; }
        Armature* a = (Armature*)o;
        Mesh* m = TypeChild<Mesh>(a, ObjectType::mesh);
        Hitbox* h = TypeChild<Hitbox>(a, ObjectType::hitbox);
        if (!m || !h) { err = "charactersshow: '" + a->name + "' no tiene su cuerpo y su sensor hijos"; return false; }
        if (m->skinArmature != a) { err = "charactersshow: el cuerpo de '" + a->name + "' no esta skinneado a su armature"; return false; }
        if (W3dHitboxOwner(h) != a) { err = "charactersshow: el dueno del sensor de '" + a->name + "' no es el armature"; return false; }
        arms.push_back(a); bodies.push_back(m);
    }
    // MALLA: un recurso, la geometria de reposo es la MISMA memoria
    MeshResource* r = bodies[0]->mesh;
    if (!r) { err = "charactersshow: el cuerpo no usa un recurso de malla"; return false; }
    for (int i = 0; i < n; i++) {
        if (bodies[i]->mesh != r) { err = "charactersshow: '" + bodies[i]->name + "' usa otro recurso de malla"; return false; }
        if (!bodies[i]->vertex || bodies[i]->vertex != r->vertex) { err = "charactersshow: la geometria de '" + bodies[i]->name + "' no es la del recurso"; return false; }
    }
    // ANIMSET: el mismo recurso y los mismos clips en memoria
    const W3dResource* rec0 = W3dArmatureAnimSetResource(arms[0]);
    int withSet = 0;
    for (int i = 0; i < n; i++) {
        const W3dResource* rec = W3dArmatureAnimSetResource(arms[i]);
        if (rec) withSet++;
        if (shared == 1) {
            if (!rec || rec != rec0) { err = "charactersshow: '" + arms[i]->name + "' no usa el MISMO animset que el primero"; return false; }
            if (arms[i]->animations.size() != arms[0]->animations.size()) { err = "charactersshow: distinta cantidad de clips"; return false; }
            for (size_t c = 0; c < arms[i]->animations.size(); c++)
                if (arms[i]->animations[c] != arms[0]->animations[c]) { err = "charactersshow: los clips de '" + arms[i]->name + "' no son los del animset compartido"; return false; }
        }
        if (shared == 0 && rec) { err = "charactersshow: '" + arms[i]->name + "' usa un animset (se esperaban clips propios)"; return false; }
    }
    // POSES por instancia en el cuadro 'frame', como en el JUEGO (ActiveAnimKind 2): cada armature
    // reproduce SU clip activo con SU cabezal (gameFrame). En el editor solo se mueve el armature
    // activo del timeline; aca se quiere ver a todos a la vez.
    const int kindPrev = ActiveAnimKind;
    std::vector<float> playheadPrev;
    for (int i = 0; i < n; i++) playheadPrev.push_back(arms[i]->gameFrame);
    ActiveAnimKind = 2;
    for (int i = 0; i < n; i++) {
        Armature* a = arms[i];
        if (a->activeAnim < 0 || a->activeAnim >= (int)a->animations.size()) { ActiveAnimKind = kindPrev; err = "charactersshow: '" + a->name + "' sin clip activo"; return false; }
        a->gameFrame = (float)(frame - a->animations[a->activeAnim]->startFrame);
        EvaluatePoseSkeleton(a, frame);
        bodies[i]->lastSkinFrame = -999999;
        SkinMesh(bodies[i]);
        if (!bodies[i]->skinVertex) { ActiveAnimKind = kindPrev; err = "charactersshow: el skinning de '" + bodies[i]->name + "' no produjo pose"; return false; }
    }
    // el editor vuelve a lo suyo: su modo, los cabezales y la pose se recalcula en el proximo frame
    ActiveAnimKind = kindPrev;
    for (int i = 0; i < n; i++) { arms[i]->gameFrame = playheadPrev[i]; arms[i]->lastPoseFrame = -999999; }
    const size_t bytes = (size_t)bodies[0]->vertexSize * 3 * sizeof(GLfloat);
    int equal = 0, distinct = 0;
    for (int i = 1; i < n; i++) {
        if (bodies[i]->skinVertex == bodies[0]->skinVertex) { err = "charactersshow: dos cuerpos comparten el buffer de pose"; return false; }
        const Mesh* ref = bodies[i % 2];   // el de su misma paridad (el mismo clip activo)
        if (i >= 2) {
            if (memcmp(bodies[i]->skinVertex, ref->skinVertex, bytes) != 0) {
                err = "charactersshow: '" + bodies[i]->name + "' con el mismo clip que '" + ref->name + "' no quedo con la misma pose"; return false;
            }
            equal++;
        }
        if (i % 2 == 1) {
            if (memcmp(bodies[i]->skinVertex, bodies[0]->skinVertex, bytes) == 0) {
                err = "charactersshow: '" + bodies[i]->name + "' con otro clip quedo con la misma pose que el primero"; return false;
            }
            distinct++;
        }
    }
    printf("      [personajesver] %d personajes: malla '%s' (reposo compartido), animset %s (%d con animset), poses: %d iguales por clip, %d distintas\n",
           n, r->name.c_str(), rec0 ? "COMPARTIDO" : "-", withSet, equal, distinct);
    return true;
}

// ---------------------------------------------------------------------------
//  personajeslog
// ---------------------------------------------------------------------------
bool CmdCharactersLog(std::istringstream& ss, std::string& err) {
    std::string dir, name, expected; ss >> dir >> name >> expected;
    if (dir.empty() || name.empty() || expected.empty()) { err = "characterslog: uso: characterslog <carpeta> <nombre> <esperado> [<esperado2>...]"; return false; }
    std::vector<std::string> expectedList(1, expected);
    { std::string t; while (ss >> t) expectedList.push_back(t); }
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "characterslog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    // el juez pide quit() al terminar; el timeout es la red de seguridad
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "characterslog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string verdict;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[personajes]");
        if (p) verdict = p;
    }
    fclose(f);
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [personajeslog] salida=%d | %s\n", r, verdict.empty() ? "(sin veredicto del juez)" : verdict.c_str());
    if (verdict.empty()) { err = "characterslog: el juez no escribio su veredicto (los scripts no corrieron?)"; return false; }
    for (size_t i = 0; i < expectedList.size(); i++)
        if (verdict.find("[personajes] OK") == std::string::npos || verdict.find(expectedList[i]) == std::string::npos) {
            err = "characterslog: en el juego compilado los eventos, los clips o las poses NO dan lo mismo que en el Play (se esperaba '" + expectedList[i] + "')";
            return false;
        }
    return true;
}

// ---------------------------------------------------------------------------
//  dupjerarquia
// ---------------------------------------------------------------------------
void CollectSubtree(Object* o, std::vector<Object*>& out) {
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) if (o->Childrens[i]) CollectSubtree(o->Childrens[i], out);
}

bool CmdDupHierarchy(std::istringstream& ss, std::string& err) {
    std::string mode, t; ss >> mode;
    std::vector<std::string> names;
    bool withChildren = false;
    while (ss >> t) { if (t == "+hijos") withChildren = true; else names.push_back(t); }
    if ((mode != "linked" && mode != "copy") || names.empty()) {
        err = "duphierarchy: uso: duphierarchy vinculado|copia <objeto>... [+hijos]"; return false;
    }
    std::vector<Object*> pick;
    for (size_t i = 0; i < names.size(); i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, names[i]) : NULL;
        if (!o) { err = "duphierarchy: no hay un objeto '" + names[i] + "'"; return false; }
        if (withChildren) CollectSubtree(o, pick); else pick.push_back(o);
    }
    std::vector<Object*> prev;
    if (SceneCollection) CollectSubtree(SceneCollection, prev);
    InteractionMode = ObjectMode;
    state = editNavigation;
    DeselectAll();
    for (size_t i = 0; i < pick.size(); i++) pick[i]->Select();
    ActiveObj = pick[0];
    if (mode == "linked") NewInstance(); else DuplicatedObject();
    state = editNavigation;   // los dos dejan agarrado el MOVER (como el teclado): se suelta
    std::vector<Object*> now;
    if (SceneCollection) CollectSubtree(SceneCollection, now);
    const int newValue = (int)now.size() - (int)prev.size();
    std::string picked;
    for (size_t i = 0; i < ObjSelects.size(); i++) picked += (i ? ", " : "") + ObjSelects[i]->name;
    printf("      [dupjerarquia] %s de %d objeto(s): %d nuevo(s); elegidas: %s\n", mode.c_str(), (int)pick.size(),
           newValue, picked.empty() ? "-" : picked.c_str());
    if (newValue <= 0) { err = "duphierarchy: no se creo nada"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  clipactivo
// ---------------------------------------------------------------------------
bool CmdClipActive(std::istringstream& ss, std::string& err) {
    std::string name, clip; ss >> name >> clip;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
    if (!o || o->getType() != ObjectType::armature || clip.empty()) { err = "clipactive: uso: clipactive <armature> <clip>"; return false; }
    Armature* a = (Armature*)o;
    for (size_t i = 0; i < a->animations.size(); i++)
        if (a->animations[i] && a->animations[i]->name == clip) {
            a->activeAnim = (int)i;
            a->lastPoseFrame = -999999; a->lastPoseAnim = -999;
            printf("      [clipactivo] '%s' -> '%s' (%d)\n", name.c_str(), clip.c_str(), (int)i);
            return true;
        }
    err = "clipactive: '" + name + "' no tiene un clip '" + clip + "'";
    return false;
}

// ---------------------------------------------------------------------------
//  objrel
// ---------------------------------------------------------------------------
const char* TypeName(Object* o) {
    switch (o->getType()) {
        case ObjectType::armature: return "armature";
        case ObjectType::mesh:     return "mesh";
        case ObjectType::hitbox:   return "hitbox";
        case ObjectType::instance: return "instance";
        case ObjectType::empty:    return "empty";
        default:                   return "other";
    }
}

bool CmdObjRel(std::istringstream& ss, std::string& err) {
    std::string name; ss >> name;
    if (name.empty()) { err = "objrel: uso: objrel <objeto> [nada] [padre P|-] [skin A|-] [hijos N] [tipo T]"; return false; }
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
    std::string k;
    if (!o) {
        printf("      [objrel] '%s': NO esta en la escena\n", name.c_str());
        while (ss >> k) if (k != "none") { err = "objrel: no hay un objeto '" + name + "'"; return false; }
        return true;
    }
    const std::string parent = (o->Parent && o->Parent != SceneCollection) ? o->Parent->name : std::string("-");
    std::string skin = "-", skinMod = "-";
    if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        if (m->skinArmature) skin = m->skinArmature->name;
        for (size_t i = 0; i < m->modifiers.size(); i++)
            if (m->modifiers[i] && m->modifiers[i]->type == ModifierType::Armature && m->modifiers[i]->target)
                skinMod = m->modifiers[i]->target->name;
    }
    printf("      [objrel] '%s' tipo=%s padre=%s skin=%s (modificador %s) hijos=%d\n", name.c_str(), TypeName(o),
           parent.c_str(), skin.c_str(), skinMod.c_str(), (int)o->Childrens.size());
    while (ss >> k) {
        std::string v;
        if (k == "none") { err = "objrel: '" + name + "' esta en la escena"; return false; }
        if (!(ss >> v)) { err = "objrel: falta el valor de '" + k + "'"; return false; }
        if (k == "parent") { if (parent != v) { err = "objrel: '" + name + "' cuelga de '" + parent + "' (se esperaba '" + v + "')"; return false; } }
        else if (k == "skin") {
            if (skin != v || skinMod != v) { err = "objrel: '" + name + "' se deforma con '" + skin + "' (modificador '" + skinMod + "'), se esperaba '" + v + "'"; return false; }
        }
        else if (k == "children") {
            if ((int)o->Childrens.size() != atoi(v.c_str())) {
                char b[160]; snprintf(b, sizeof(b), "objrel: '%s' tiene %d hijos (se esperaban %s)", name.c_str(), (int)o->Childrens.size(), v.c_str());
                err = b; return false;
            }
        }
        else if (k == "type") { if (v != TypeName(o)) { err = "objrel: '" + name + "' es " + TypeName(o) + " (se esperaba " + v + ")"; return false; } }
        else { err = "objrel: argumento desconocido '" + k + "'"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  personajesdirector
// ---------------------------------------------------------------------------
bool CmdCharactersDirector(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "charactersdirector: uso: charactersdirector <carpeta>"; return false; }
    if (!SceneCollection || !FindObjectByName(SceneCollection, CharacterName(1))) {
        err = "charactersdirector: falta la escena de 'personajesmin <carpeta> 2' (o mas)"; return false;
    }
    if (!Write(dir + "/pj_director.lua", kLuaDirector)) { err = "charactersdirector: no pude escribir el .lua en " + dir; return false; }
    Empty* d = new Empty(NULL, Vector3(0.0f, -12.0f, 0.0f));
    d->SetNameObj("Director");
    Script(d, "pj_director.lua");
    // el PASEANTE y su animacion de escena "Paseo" (x de -20 a -10 en 30 frames): la mezcla la
    // capa de escena que pide el Director. La escena activa del timeline no cambia.
    Empty* pas = new Empty(NULL, Vector3(-20.0f, 0.0f, -8.0f));
    pas->SetNameObj("Paseante");
    InitSceneAnimations();
    const int active = ActiveSceneAnim;
    const int idx = NewScene();
    RenameActiveScene("Paseo");
    SceneAnimations[idx]->startFrame = 1; SceneAnimations[idx]->endFrame = 30; SceneAnimations[idx]->fps = 30;
    { AnimationObject ao; ao.obj = pas;
      AnimProperty& px = ListProperty(ao.Properties, AnimPosition, AnimX);
      SetKeyCurve(px, 1, -20.0f); SetKeyCurve(px, 30, -10.0f);
      ao.UpdateFirstLastFrame();
      AnimationObjects.push_back(ao); }
    SetActiveScene(active);
    printf("      [personajesdirector] 'Director' con pj_director.lua + 'Paseante' con su escena 'Paseo' en '%s'\n", dir.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  esqjuego
// ---------------------------------------------------------------------------
bool CmdEsqGame(std::istringstream& ss, std::string& err) {
    std::string name; ss >> name;
    if (name.empty()) {
        err = "armaturegame: uso: armaturegame <armature>|escenas [clip C|-] [frame F] [vel V] [loop 0|1] [termino 0|1] [capas N] [trans 0|1]";
        return false;
    }
    std::string k;
    if (name == "escenas") {
        printf("      [esqjuego] mix de escenas: %d capa(s)\n", (int)g_mixScenes.size());
        while (ss >> k) {
            if (k != "layers") { err = "armaturegame escenas: argumento desconocido '" + k + "'"; return false; }
            int n = -1; ss >> n;
            if ((int)g_mixScenes.size() != n) {
                char b[128]; snprintf(b, sizeof(b), "armaturegame: el mix de escenas tiene %d capa(s) (se esperaban %d)", (int)g_mixScenes.size(), n);
                err = b; return false;
            }
        }
        return true;
    }
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
    if (!o || o->getType() != ObjectType::armature) { err = "armaturegame: no hay un armature '" + name + "'"; return false; }
    Armature* a = (Armature*)o;
    const bool hasClip = a->activeAnim >= 0 && a->activeAnim < (int)a->animations.size() && a->animations[a->activeAnim];
    const std::string clip = hasClip ? a->animations[a->activeAnim]->name : std::string("-");
    const bool trans = a->remainingTrans > 0.0f;
    printf("      [esqjuego] '%s' clip=%s frame=%.3f vel=%.3f loop=%d termino=%d capas=%d trans=%.3f\n",
           name.c_str(), clip.c_str(), a->gameFrame, a->gameVel, a->gameLoop ? 1 : 0, a->gameEnded ? 1 : 0,
           (int)a->layers.size(), a->remainingTrans);
    while (ss >> k) {
        char b[200]; b[0] = 0;
        if (k == "clip") {
            std::string c; ss >> c;
            if (c != clip) snprintf(b, sizeof(b), "el clip activo es '%s' (se esperaba '%s')", clip.c_str(), c.c_str());
        } else if (k == "frame" || k == "vel") {
            float v = -1.0f; ss >> v;
            const float real = (k == "frame") ? a->gameFrame : a->gameVel;
            if (fabsf(real - v) > 0.01f) snprintf(b, sizeof(b), "%s=%.3f (se esperaba %.3f)", k.c_str(), real, v);
        } else if (k == "loop" || k == "ended" || k == "trans") {
            int v = -1; ss >> v;
            const bool real = (k == "loop") ? a->gameLoop : (k == "ended") ? a->gameEnded : trans;
            if ((v != 0) != real) snprintf(b, sizeof(b), "%s=%d (se esperaba %d)", k.c_str(), real ? 1 : 0, v);
        } else if (k == "layers") {
            int n = -1; ss >> n;
            if ((int)a->layers.size() != n) snprintf(b, sizeof(b), "capas=%d (se esperaban %d)", (int)a->layers.size(), n);
        } else { err = "armaturegame: argumento desconocido '" + k + "'"; return false; }
        if (b[0]) { err = "armaturegame: '" + name + "' " + b; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  mixescena
// ---------------------------------------------------------------------------
bool CmdMixScene(std::istringstream& ss, std::string& err) {
    std::string sub, layer, name; ss >> sub;
    if (sub == "mix") {
        int frame = 1; ss >> layer >> frame >> name;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
        if (layer.empty() || !o) { err = "mixscene: uso: mixscene mezclar <capa> <frame> <objeto>"; return false; }
        bool is = false;
        for (size_t k = 0; k < g_mixScenes.size(); k++) if (g_mixScenes[k].anim == layer) is = true;
        if (!is) { W3dLayerAnim c; c.anim = layer; g_mixScenes.push_back(c); }
        const Vector3 prev = o->pos;
        W3dMixScenesApply(frame, false);
        printf("      [mixescena] mezclar '%s' en %d: '%s' (%.3f,%.3f,%.3f) -> (%.3f,%.3f,%.3f)\n", layer.c_str(), frame,
               name.c_str(), prev.x, prev.y, prev.z, o->pos.x, o->pos.y, o->pos.z);
        if (o->pos.x == prev.x && o->pos.y == prev.y && o->pos.z == prev.z) {
            err = "mixscene: la capa '" + layer + "' no movio a '" + name + "' (no la toca?)"; return false;
        }
        return true;
    }
    if (sub == "release") {
        float x = 0, y = 0, z = 0; ss >> name >> x >> y >> z;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
        if (!o) { err = "mixscene: uso: mixscene soltar <objeto> <x> <y> <z>"; return false; }
        W3dMixScenesRelease();
        g_mixScenes.clear();
        printf("      [mixescena] soltar: '%s' en (%.3f,%.3f,%.3f)\n", name.c_str(), o->pos.x, o->pos.y, o->pos.z);
        if (fabsf(o->pos.x - x) > 0.001f || fabsf(o->pos.y - y) > 0.001f || fabsf(o->pos.z - z) > 0.001f) {
            char b[200]; snprintf(b, sizeof(b), "mixscene: al soltar, '%s' quedo en (%.3f,%.3f,%.3f) y su base es (%.3f,%.3f,%.3f)",
                                  name.c_str(), o->pos.x, o->pos.y, o->pos.z, x, y, z);
            err = b; return false;
        }
        return true;
    }
    err = "mixscene: uso: mixscene mezclar <capa> <frame> <objeto> | mixescena soltar <objeto> <x> <y> <z>";
    return false;
}

// ---------------------------------------------------------------------------
//  reporaiz
// ---------------------------------------------------------------------------
std::string RealPath(const std::string& r) {
#ifdef _WIN32
    char b[4096];
    return _fullpath(b, r.c_str(), sizeof(b)) ? std::string(b) : r;
#else
    char b[4096];
    return realpath(r.c_str(), b) ? std::string(b) : r;
#endif
}
// pone (o saca, con v NULL) una variable de entorno del proceso
void SetEnvironment(const char* k, const char* v) {
#ifdef _WIN32
    _putenv_s(k, v ? v : "");
#else
    if (v) setenv(k, v, 1); else unsetenv(k);
#endif
}

bool CmdRepoRoot(std::istringstream& ss, std::string& err) {
    std::string expected; ss >> expected;
    if (expected.empty()) { err = "reporoot: uso: reporoot <esperado> [ajustes <ruta>|-] [entorno <ruta>|-] [via <camino>]"; return false; }
    bool setSettings = false, setEnvironment = false;
    std::string settings, environment, via, k;
    while (ss >> k) {
        if (k == "settings")      { ss >> settings; setSettings = true; if (settings == "-") settings.clear(); }
        else if (k == "environment") { ss >> environment; setEnvironment = true; if (environment == "-") environment.clear(); }
        else if (k == "via")     ss >> via;
        else { err = "reporoot: argumento desconocido '" + k + "'"; return false; }
    }
    // lo de ahora se guarda y se repone al salir (el resto de la corrida no se entera)
    const std::string settingsPrev = cfg.repoPath;
    const char* e0 = getenv("W3D_REPO");
    const bool hadEnvironment = (e0 != NULL);
    const std::string environmentPrev = e0 ? e0 : "";
    if (setSettings) cfg.repoPath = settings;
    if (setEnvironment) SetEnvironment("W3D_REPO", environment.empty() ? NULL : environment.c_str());
    std::string asWas;
    const std::string root = CompileGameRepoRoot(&asWas);
    cfg.repoPath = settingsPrev;
    if (setEnvironment) SetEnvironment("W3D_REPO", hadEnvironment ? environmentPrev.c_str() : NULL);
    printf("      [reporaiz] '%s' (via %s)\n", root.c_str(), asWas.empty() ? "-" : asWas.c_str());
    if (root.empty()) { err = "reporoot: no encontro la raiz del repo"; return false; }
    if (RealPath(root) != RealPath(expected)) {
        err = "reporoot: encontro '" + RealPath(root) + "' y se esperaba '" + RealPath(expected) + "'"; return false;
    }
    if (!via.empty() && via != asWas) { err = "reporoot: la dio '" + asWas + "' y se esperaba '" + via + "'"; return false; }
    return true;
}

} // namespace

// ============================================================================
//  el despachador
// ============================================================================
bool W3dTestsIntegrationCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "charactersmin") return CmdCharactersMin(ss, err);
    if (cmd == "charactersshow") return CmdCharactersVer(ss, err);
    if (cmd == "characterslog") return CmdCharactersLog(ss, err);
    if (cmd == "charactersdirector") return CmdCharactersDirector(ss, err);
    if (cmd == "duphierarchy")  return CmdDupHierarchy(ss, err);
    if (cmd == "objrel")        return CmdObjRel(ss, err);
    if (cmd == "clipactive")    return CmdClipActive(ss, err);
    if (cmd == "armaturegame")      return CmdEsqGame(ss, err);
    if (cmd == "mixscene")     return CmdMixScene(ss, err);
    if (cmd == "reporoot")      return CmdRepoRoot(ss, err);
    handled = false;
    return false;
}
