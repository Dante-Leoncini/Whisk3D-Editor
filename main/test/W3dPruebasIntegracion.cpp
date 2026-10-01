// ============================================================================
//  W3dPruebasIntegracion.cpp — la prueba CRUZADA de tres areas que se hicieron por
//  separado: las MALLAS como recurso compartido (objects/MallaRecurso.h), los
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
//            alQuedarse/alSalir en compartido("p<k>_...").
//        Los DEMAS son el personaje entero duplicado con Alt+D (esqueleto + cuerpo + sensor
//        elegidos, NewInstance: la misma puerta que el teclado): cada copia es otro esqueleto
//        con los MISMOS clips en memoria (un animset), otro cuerpo con la MISMA malla
//        skinneado a SU esqueleto y otro sensor hijo. Se ubica en la fila, alterna el clip
//        activo (par = Saludo, impar = Quieto: poses distintas con los mismos clips) y cambia
//        el prefijo de su contador.
//        Un "Jugador" (vacio con un hitbox hijo etiqueta "jugador") pasa por delante de
//        todos a paso FIJO por tick y cuenta lo suyo en compartido("jugador_..."), y un
//        "Juez" deja el veredicto en compartido("juez") y en el log ("[personajes] OK ...")
//        y pide salir(): el mismo juez en el Play del editor y en el juego compilado.
//        Tambien exige que animLargo vea los clips de TODOS los personajes (en el juego,
//        que el .w3da haya llegado) y que los esqueletos se MUEVAN: la punta del brazo del
//        primero cambia entre el arranque y un cabezal avanzado (compartido("pose") y
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
//        Animation): 'personajesver' compara las poses de los que tienen el mismo clip activo.
//    objrel <objeto> [nada] [padre P|-] [skin A|-] [hijos N] [tipo T]
//        el lugar de un objeto en el arbol: 'nada' = no esta en la escena; 'padre -' = cuelga
//        de la raiz; 'skin' = el esqueleto que lo deforma (Mesh::skinArmature Y el target de su
//        modificador Armature, que tienen que coincidir); 'hijos' = cuantos hijos directos;
//        'tipo' = armature | mesh | hitbox | instance | empty.
//    personajesdirector <carpeta>
//        agrega a la escena de personajesmin (n >= 2) un "Director" cuyo inicio() cambia
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
//        (W3dMixEscenasSoltar), saca las capas y exige que el objeto quede en (x,y,z): la
//        BASE que el mix anoto, o sea donde estaba antes de mezclarlo (tol 0.001).
//    reporaiz <esperado> [ajustes <ruta>|-] [entorno <ruta>|-] [via <camino>]
//        la raiz del repo que usa "Compilar juego" (CompilarJuegoRepoRaiz) tiene que ser la
//        carpeta 'esperado' (se comparan las rutas reales). 'ajustes' y 'entorno' ponen, SOLO
//        durante el comando, cfg.repoPath y la variable W3D_REPO ('-' = vacias): una ruta que
//        no es el repo se tiene que saltear. 'via' exige el camino que la dio (ajustes,
//        entorno, res, binario o fuentes).
// ============================================================================
#include "test/W3dPruebasIntegracion.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Armature.h"
#include "objects/Empty.h"
#include "objects/Hitbox.h"
#include "objects/ObjectMode.h"        // NewInstance (Alt+D)
#include "io/MallasProyecto.h"         // W3dMallaCrearRecurso
#include "io/W3dRecursos.h"
#include "animation/SkeletalAnimation.h"
#include "animation/Animation.h"       // ActiveAnimKind (las poses como en el juego)
#include "animation/W3dAnimSet.h"      // W3dArmatureAnimSetRecurso
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"          // WeightPaintAsegurarMapa
#include "script/W3dScript.h"
#include "base/W3dInteractionState.h"  // Alt+D exige Modo Objeto navegando
#include "io/CompilarJuego.h"        // CompilarJuegoRepoRaiz (reporaiz)
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
// el contador de eventos de hitbox en compartido("<prefijo>_...")
const char* kLuaContador =
    "-- contador de eventos de hitbox (prueba cruzada del motor)\n"
    "propiedades = { prefijo = \"x\" }\n"
    "local p = \"x\"\n"
    "local function suma(c) local k = p .. \"_\" .. c; setCompartido(k, (compartido(k) or 0) + 1) end\n"
    "function inicio()\n"
    "  p = propiedad(\"prefijo\")\n"
    "  setCompartido(p .. \"_entrar\", 0); setCompartido(p .. \"_quedarse\", 0); setCompartido(p .. \"_salir\", 0)\n"
    "end\n"
    "function alEntrar(otro, mio, suyo) suma(\"entrar\"); if otro then setCompartido(p .. \"_otro\", nombre(otro)) end end\n"
    "function alQuedarse(otro, mio, suyo) suma(\"quedarse\") end\n"
    "function alSalir(otro, mio, suyo) suma(\"salir\") end\n";
// avanza en X un paso FIJO por tick (determinista aunque el dt del juego varie)
const char* kLuaMover =
    "-- avanza en X un paso fijo por tick (x = x0 + paso * n)\n"
    "propiedades = { x0 = 0, paso = 0.4, y = 0, z = 0 }\n"
    "local n = 0\n"
    "function actualizar(dt)\n"
    "  n = n + 1\n"
    "  setPosicion(yo(), propiedad(\"x0\") + propiedad(\"paso\") * n, propiedad(\"y\"), propiedad(\"z\"))\n"
    "end\n";
// EL JUEZ: cuando el jugador paso a todos, mira los contadores y los clips de cada personaje, y
// que los esqueletos se MUEVAN: la punta del brazo del primero (clip Saludo) al arrancar contra la
// de un cabezal avanzado (frame >= 8). Un juego que no reproduce los clips (esqueletos en bind)
// deja las dos iguales: "pose=quieta" y FALTA.
const char* kLuaJuez =
    "-- veredicto de la prueba cruzada (malla + animset compartidos + hitbox)\n"
    "propiedades = { n = 1, fin = 0, largo = 0, largo2 = 0 }\n"
    "local listo = false\n"
    "local ticks = 0\n"
    "local x1, y1, z1\n"
    "local function c(k) return compartido(k) or -1 end\n"
    "local function nom(k) if k == 0 then return \"Personaje\" end return string.format(\"Personaje.%03d\", k) end\n"
    "function actualizar(dt)\n"
    "  if listo then return end\n"
    "  ticks = ticks + 1\n"
    "  local a0 = buscar(\"Personaje\")\n"
    "  if ticks == 1 then x1, y1, z1 = huesoPunto(a0, \"brazo\", 0, 1, 0) end\n"
    "  local x = posicion(buscar(\"Jugador\"))\n"
    "  local f = animFrame(a0) or 0\n"
    "  if (x < propiedad(\"fin\") or f < 8) and ticks < 100000 then return end\n"
    "  listo = true\n"
    "  local x2, y2, z2 = huesoPunto(a0, \"brazo\", 0, 1, 0)\n"
    "  local d = -1\n"
    "  if x1 and x2 then d = math.abs(x2 - x1) + math.abs(y2 - y1) + math.abs(z2 - z1) end\n"
    "  local pose = (d > 0.01) and \"mueve\" or \"quieta\"\n"
    "  local n = propiedad(\"n\")\n"
    "  local q0 = c(\"p0_quedarse\")\n"
    "  local ok = c(\"jugador_entrar\") == n and c(\"jugador_salir\") == n and q0 >= 1\n"
    "  local clips = 0\n"
    "  for k = 0, n - 1 do\n"
    "    local p = \"p\" .. k\n"
    "    if c(p .. \"_entrar\") ~= 1 or c(p .. \"_salir\") ~= 1 or c(p .. \"_quedarse\") ~= q0 or c(p .. \"_otro\") ~= \"Jugador\" then ok = false end\n"
    "    local a = buscar(nom(k))\n"
    "    -- animLargo solo MIRA el clip (animClip lo pondria a reproducir y cambiaria las poses)\n"
    "    if a and animLargo(a, \"Saludo\") == propiedad(\"largo\") and animLargo(a, \"Quieto\") == propiedad(\"largo2\") then clips = clips + 1 end\n"
    "  end\n"
    "  if clips ~= n then ok = false end\n"
    "  if pose ~= \"mueve\" then ok = false end\n"
    "  local linea = string.format(\"[personajes] %s n=%d p=%d/%d/%d jugador=%d/%d clips=%d pose=%s d=%.3f frame=%.1f ticks=%d\",\n"
    "    ok and \"OK\" or \"FALTA\", n, c(\"p0_entrar\"), q0, c(\"p0_salir\"), c(\"jugador_entrar\"), c(\"jugador_salir\"), clips, pose, d, f, ticks)\n"
    "  info(linea)\n"
    "  setCompartido(\"pose\", pose)\n"
    "  setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "  salir()\n"
    "end\n";
// EL DIRECTOR: cambia en juego lo que la API lua de esqueletos puede cambiar (personajesdirector)
const char* kLuaDirector =
    "-- cambia EN JUEGO el estado de animacion de los esqueletos (prueba del Stop y del rewind)\n"
    "function inicio()\n"
    "  local a = buscar(\"Personaje\")\n"
    "  local b = buscar(\"Personaje.001\")\n"
    "  animClip(a, \"Quieto\", false)          -- otro clip, sin loop\n"
    "  animVelocidad(a, 2)\n"
    "  animTransicion(b, 6)                   -- 6 frames a 30 fps: 0.2 s fundiendo\n"
    "  animCapa(b, 1, \"Saludo\", 0.5, \"sumar\")\n"
    "  local p = buscar(\"Paseante\")\n"
    "  setPosicion(p, -20, 50, -8)           -- el mix lo toca desde ACA (su base en la partida)\n"
    "  escenaCapa(1, \"Paseo\", 0.25)          -- una capa del mix de ESCENAS (global)\n"
    "end\n";

bool Escribir(const std::string& ruta, const char* texto) {
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) return false;
    fputs(texto, f);
    fclose(f);
    return true;
}

void Script(Object* o, const char* ruta) {
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e; e.ruta = ruta;
    o->scriptDatos->scripts.push_back(e);
}
void Ref(Object* o, const std::string& prop, const std::string& valor) {
    o->scriptDatos->scripts.back().refs.push_back(std::make_pair(prop, valor));
}
std::string Num(float v) { char b[48]; snprintf(b, sizeof(b), "%g", v); return b; }

// un clip de un solo hueso: rotacion lineal de 'a' a 'b' grados en el eje 'comp'
SkeletalAnimation* Clip(const char* nombre, int hueso, int comp, float a, float b, int fin) {
    SkeletalAnimation* c = new SkeletalAnimation(nombre);
    c->FrameRate = 24; c->startFrame = 1; c->endFrame = fin;
    BoneTrack tr; tr.bone = hueso;
    AnimProperty ap; ap.Property = AnimRotation; ap.component = comp;
    keyFrame k0; k0.frame = 1;   k0.value = a; k0.Interpolation = KfLinear; ap.keyframes.push_back(k0);
    keyFrame k1; k1.frame = fin; k1.value = b; k1.Interpolation = KfLinear; ap.keyframes.push_back(k1);
    tr.Propertys.push_back(ap);
    c->tracks.push_back(tr);
    return c;
}

// el esqueleto de un personaje (2 huesos) con sus dos clips (IGUALES en todos)
const int kLargoSaludo = 20;
const int kLargoQuieto = 30;
Armature* NuevoEsqueleto(const Vector3& pos, int k) {
    Armature* a = new Armature(NULL, pos);
    a->SetNameObj("Personaje");
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0, 0, 0); r.tail = Vector3(0, 1, 0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0, 1, 0); c.tail = Vector3(0, 2, 0); a->bones.push_back(c); }
    PrepararSkinAutorado(a);
    a->animations.push_back(Clip("Saludo", 1, AnimZ, 0.0f, 60.0f, kLargoSaludo));
    a->animations.push_back(Clip("Quieto", 0, AnimY, 0.0f, 35.0f, kLargoQuieto));
    a->animActiva = k % 2;   // par = Saludo, impar = Quieto
    return a;
}

// el cuerpo del primer personaje: un cilindro con los pesos por altura
Mesh* NuevoCuerpo(Armature* a) {
    Mesh* s = (Mesh*)NewMesh(MeshType::cylinder, a, false);
    if (!s) return NULL;
    s->SetNameObj("Cuerpo");
    s->pos = Vector3(0, 0, 0);
    s->GenerarRender();
    WeightPaintAsegurarMapa(s);
    VertexGroup* vr = new VertexGroup("raiz");
    VertexGroup* vb = new VertexGroup("brazo");
    std::vector<char> hecho(s->vertexSize, 0);
    for (int i = 0; i < s->vertexSize; i++) {
        const int cp = s->vertCtrlPoint[i];
        if (hecho[cp]) continue;
        hecho[cp] = 1;
        if (s->vertex[i * 3 + 1] > 0.0f) { vb->verts.push_back(cp); vb->pesos.push_back(1.0f); }
        else { vr->verts.push_back(cp); vr->pesos.push_back(1.0f); }
    }
    s->vertexGroups.push_back(vr);
    s->vertexGroups.push_back(vb);
    Modifier* md = new Modifier(ModifierType::Armature, NombreTipoModificador(ModifierType::Armature));
    md->target = (Object*)a;
    s->modificadores.push_back(md);
    s->skinArmature = a;
    return s;
}

std::string NombrePersonaje(int k) {
    if (k == 0) return "Personaje";
    char b[32]; snprintf(b, sizeof(b), "Personaje.%03d", k);
    return b;
}

template <class T> T* HijoDeTipo(Object* o, ObjectType t) {
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (o->Childrens[i] && o->Childrens[i]->getType() == t) return (T*)o->Childrens[i];
    return NULL;
}

// ---------------------------------------------------------------------------
//  personajesmin
// ---------------------------------------------------------------------------
bool CmdPersonajesMin(std::istringstream& ss, std::string& err) {
    std::string dir; int n = 0;
    ss >> dir >> n;
    if (dir.empty() || n < 1) { err = "personajesmin: uso: personajesmin <carpeta> <n>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "personajesmin: " + e2; return false; }
    if (!Escribir(dir + "/pj_contador.lua", kLuaContador) || !Escribir(dir + "/pj_mover.lua", kLuaMover) ||
        !Escribir(dir + "/pj_juez.lua", kLuaJuez)) {
        err = "personajesmin: no pude escribir los .lua en " + dir; return false;
    }
    const float z = -3.0f;
    // EL PRIMER PERSONAJE, por codigo: esqueleto con sus clips, cuerpo como recurso "Cuerpo" y el
    // sensor (Add > Hitbox con el armature activo: hijo, del tamano de su cuerpo)
    Armature* a0 = NuevoEsqueleto(Vector3(0.0f, 0.0f, z), 0);
    Mesh* cuerpo0 = NuevoCuerpo(a0);
    if (!cuerpo0) { err = "personajesmin: NewMesh fallo"; return false; }
    if (!W3dMallaCrearRecurso(cuerpo0, "Cuerpo")) { err = "personajesmin: no pude crear el recurso 'Cuerpo'"; return false; }
    Hitbox* sensor0 = HitboxCrearAjustado(a0);
    if (!sensor0) { err = "personajesmin: HitboxCrearAjustado fallo"; return false; }
    sensor0->SetNameObj("Sensor");
    sensor0->filtro = "jugador";
    Script(a0, "pj_contador.lua"); Ref(a0, "prefijo", "p0");
    // LOS DEMAS son su DUPLICADO VINCULADO: Alt+D con el esqueleto, el cuerpo y el sensor elegidos,
    // por la MISMA puerta que el teclado (Modo Objeto, navegando). Cada copia es otro personaje:
    // otro esqueleto con los MISMOS clips (los del primero pasan a un animset en memoria), otro
    // cuerpo con la MISMA malla re-skinneado a SU esqueleto y otro sensor hijo. Despues se ubica en
    // la fila, alterna el clip activo (par = Saludo, impar = Quieto) y cambia el prefijo de su
    // contador (el script viene copiado con los valores del original, como en el editor).
    for (int k = 1; k < n; k++) {
        InteractionMode = ObjectMode;
        estado = editNavegacion;
        DeseleccionarTodo();
        a0->Seleccionar(); cuerpo0->Seleccionar(); sensor0->Seleccionar();
        ObjActivo = a0;
        NewInstance();
        estado = editNavegacion;   // Alt+D deja agarrado el MOVER (como el teclado): se suelta
        if (!ObjActivo || ObjActivo == a0 || ObjActivo->getType() != ObjectType::armature) {
            err = "personajesmin: Alt+D no creo otro esqueleto"; return false;
        }
        Armature* a = (Armature*)ObjActivo;
        if (ObjSelects.size() != 1) { err = "personajesmin: Alt+D dejo elegido algo mas que el esqueleto nuevo"; return false; }
        a->pos = Vector3(4.0f * (float)k, 0.0f, z);
        a->animActiva = k % 2;
        char pref[32]; snprintf(pref, sizeof(pref), "p%d", k);
        if (!a->scriptDatos || a->scriptDatos->scripts.empty()) { err = "personajesmin: la copia no trajo su script"; return false; }
        std::vector<std::pair<std::string, std::string> >& refs = a->scriptDatos->scripts[0].refs;
        for (size_t r = 0; r < refs.size(); r++) if (refs[r].first == "prefijo") refs[r].second = pref;
    }
    // el jugador: pasa por delante de todos (los personajes estan cada 4 m; 4 / 0.4 = 10 ticks)
    const float x0 = -3.05f;
    Empty* jugador = new Empty(NULL, Vector3(x0, 0.0f, z));
    jugador->SetNameObj("Jugador");
    Script(jugador, "pj_mover.lua");
    Ref(jugador, "x0", Num(x0)); Ref(jugador, "z", Num(z));
    Script(jugador, "pj_contador.lua"); Ref(jugador, "prefijo", "jugador");
    Hitbox* caja = new Hitbox(jugador, Vector3(0, 0, 0));
    caja->SetNameObj("CajaJugador");
    caja->tam[0] = caja->tam[1] = caja->tam[2] = 0.5f;
    caja->etiqueta = "jugador";
    // el juez: veredicto cuando el jugador dejo atras al ultimo personaje
    Empty* juez = new Empty(NULL, Vector3(0.0f, -10.0f, 0.0f));
    juez->SetNameObj("Juez");
    Script(juez, "pj_juez.lua");
    { char b[16]; snprintf(b, sizeof(b), "%d", n); Ref(juez, "n", b); }
    Ref(juez, "fin", Num(4.0f * (float)(n - 1) + 3.0f));
    { char b[16]; snprintf(b, sizeof(b), "%d", kLargoSaludo); Ref(juez, "largo", b); }
    { char b[16]; snprintf(b, sizeof(b), "%d", kLargoQuieto); Ref(juez, "largo2", b); }
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [personajesmin] juego3dmin + %d personajes (el primero por codigo, los demas su Alt+D entero: esqueleto + cuerpo + sensor) + jugador + juez en '%s'\n",
           n, dir.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  personajesver
// ---------------------------------------------------------------------------
bool CmdPersonajesVer(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 2) { err = "personajesver: uso: personajesver <n> [compartido 0|1] [frame F] (n >= 2)"; return false; }
    int compartido = -1, frame = 10;
    std::string k;
    while (ss >> k) {
        if (k == "compartido") ss >> compartido;
        else if (k == "frame") ss >> frame;
        else { err = "personajesver: argumento desconocido '" + k + "'"; return false; }
    }
    std::vector<Armature*> arms;
    std::vector<Mesh*> cuerpos;
    for (int i = 0; i < n; i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, NombrePersonaje(i)) : NULL;
        if (!o || o->getType() != ObjectType::armature) { err = "personajesver: falta el armature '" + NombrePersonaje(i) + "'"; return false; }
        Armature* a = (Armature*)o;
        Mesh* m = HijoDeTipo<Mesh>(a, ObjectType::mesh);
        Hitbox* h = HijoDeTipo<Hitbox>(a, ObjectType::hitbox);
        if (!m || !h) { err = "personajesver: '" + a->name + "' no tiene su cuerpo y su sensor hijos"; return false; }
        if (m->skinArmature != a) { err = "personajesver: el cuerpo de '" + a->name + "' no esta skinneado a su armature"; return false; }
        if (W3dHitboxDuenio(h) != a) { err = "personajesver: el dueno del sensor de '" + a->name + "' no es el armature"; return false; }
        arms.push_back(a); cuerpos.push_back(m);
    }
    // MALLA: un recurso, la geometria de reposo es la MISMA memoria
    MallaRecurso* r = cuerpos[0]->malla;
    if (!r) { err = "personajesver: el cuerpo no usa un recurso de malla"; return false; }
    for (int i = 0; i < n; i++) {
        if (cuerpos[i]->malla != r) { err = "personajesver: '" + cuerpos[i]->name + "' usa otro recurso de malla"; return false; }
        if (!cuerpos[i]->vertex || cuerpos[i]->vertex != r->vertex) { err = "personajesver: la geometria de '" + cuerpos[i]->name + "' no es la del recurso"; return false; }
    }
    // ANIMSET: el mismo recurso y los mismos clips en memoria
    const W3dRecurso* rec0 = W3dArmatureAnimSetRecurso(arms[0]);
    int conSet = 0;
    for (int i = 0; i < n; i++) {
        const W3dRecurso* rec = W3dArmatureAnimSetRecurso(arms[i]);
        if (rec) conSet++;
        if (compartido == 1) {
            if (!rec || rec != rec0) { err = "personajesver: '" + arms[i]->name + "' no usa el MISMO animset que el primero"; return false; }
            if (arms[i]->animations.size() != arms[0]->animations.size()) { err = "personajesver: distinta cantidad de clips"; return false; }
            for (size_t c = 0; c < arms[i]->animations.size(); c++)
                if (arms[i]->animations[c] != arms[0]->animations[c]) { err = "personajesver: los clips de '" + arms[i]->name + "' no son los del animset compartido"; return false; }
        }
        if (compartido == 0 && rec) { err = "personajesver: '" + arms[i]->name + "' usa un animset (se esperaban clips propios)"; return false; }
    }
    // POSES por instancia en el cuadro 'frame', como en el JUEGO (ActiveAnimKind 2): cada armature
    // reproduce SU clip activo con SU cabezal (juegoFrame). En el editor solo se mueve el armature
    // activo del timeline; aca se quiere ver a todos a la vez.
    const int kindAntes = ActiveAnimKind;
    std::vector<float> cabezalAntes;
    for (int i = 0; i < n; i++) cabezalAntes.push_back(arms[i]->juegoFrame);
    ActiveAnimKind = 2;
    for (int i = 0; i < n; i++) {
        Armature* a = arms[i];
        if (a->animActiva < 0 || a->animActiva >= (int)a->animations.size()) { ActiveAnimKind = kindAntes; err = "personajesver: '" + a->name + "' sin clip activo"; return false; }
        a->juegoFrame = (float)(frame - a->animations[a->animActiva]->startFrame);
        EvaluarPoseEsqueleto(a, frame);
        cuerpos[i]->lastSkinFrame = -999999;
        SkinearMesh(cuerpos[i]);
        if (!cuerpos[i]->skinVertex) { ActiveAnimKind = kindAntes; err = "personajesver: el skinning de '" + cuerpos[i]->name + "' no produjo pose"; return false; }
    }
    // el editor vuelve a lo suyo: su modo, los cabezales y la pose se recalcula en el proximo frame
    ActiveAnimKind = kindAntes;
    for (int i = 0; i < n; i++) { arms[i]->juegoFrame = cabezalAntes[i]; arms[i]->lastPoseFrame = -999999; }
    const size_t bytes = (size_t)cuerpos[0]->vertexSize * 3 * sizeof(GLfloat);
    int iguales = 0, distintas = 0;
    for (int i = 1; i < n; i++) {
        if (cuerpos[i]->skinVertex == cuerpos[0]->skinVertex) { err = "personajesver: dos cuerpos comparten el buffer de pose"; return false; }
        const Mesh* ref = cuerpos[i % 2];   // el de su misma paridad (el mismo clip activo)
        if (i >= 2) {
            if (memcmp(cuerpos[i]->skinVertex, ref->skinVertex, bytes) != 0) {
                err = "personajesver: '" + cuerpos[i]->name + "' con el mismo clip que '" + ref->name + "' no quedo con la misma pose"; return false;
            }
            iguales++;
        }
        if (i % 2 == 1) {
            if (memcmp(cuerpos[i]->skinVertex, cuerpos[0]->skinVertex, bytes) == 0) {
                err = "personajesver: '" + cuerpos[i]->name + "' con otro clip quedo con la misma pose que el primero"; return false;
            }
            distintas++;
        }
    }
    printf("      [personajesver] %d personajes: malla '%s' (reposo compartido), animset %s (%d con animset), poses: %d iguales por clip, %d distintas\n",
           n, r->nombre.c_str(), rec0 ? "COMPARTIDO" : "-", conSet, iguales, distintas);
    return true;
}

// ---------------------------------------------------------------------------
//  personajeslog
// ---------------------------------------------------------------------------
bool CmdPersonajesLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre, esperado; ss >> dir >> nombre >> esperado;
    if (dir.empty() || nombre.empty() || esperado.empty()) { err = "personajeslog: uso: personajeslog <carpeta> <nombre> <esperado> [<esperado2>...]"; return false; }
    std::vector<std::string> esperados(1, esperado);
    { std::string t; while (ss >> t) esperados.push_back(t); }
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "personajeslog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    // el juez pide salir() al terminar; el timeout es la red de seguridad
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "personajeslog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string veredicto;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[personajes]");
        if (p) veredicto = p;
    }
    fclose(f);
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [personajeslog] salida=%d | %s\n", r, veredicto.empty() ? "(sin veredicto del juez)" : veredicto.c_str());
    if (veredicto.empty()) { err = "personajeslog: el juez no escribio su veredicto (los scripts no corrieron?)"; return false; }
    for (size_t i = 0; i < esperados.size(); i++)
        if (veredicto.find("[personajes] OK") == std::string::npos || veredicto.find(esperados[i]) == std::string::npos) {
            err = "personajeslog: en el juego compilado los eventos, los clips o las poses NO dan lo mismo que en el Play (se esperaba '" + esperados[i] + "')";
            return false;
        }
    return true;
}

// ---------------------------------------------------------------------------
//  dupjerarquia
// ---------------------------------------------------------------------------
void JuntarSubarbol(Object* o, std::vector<Object*>& out) {
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) if (o->Childrens[i]) JuntarSubarbol(o->Childrens[i], out);
}

bool CmdDupJerarquia(std::istringstream& ss, std::string& err) {
    std::string modo, t; ss >> modo;
    std::vector<std::string> nombres;
    bool conHijos = false;
    while (ss >> t) { if (t == "+hijos") conHijos = true; else nombres.push_back(t); }
    if ((modo != "vinculado" && modo != "copia") || nombres.empty()) {
        err = "dupjerarquia: uso: dupjerarquia vinculado|copia <objeto>... [+hijos]"; return false;
    }
    std::vector<Object*> elegir;
    for (size_t i = 0; i < nombres.size(); i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nombres[i]) : NULL;
        if (!o) { err = "dupjerarquia: no hay un objeto '" + nombres[i] + "'"; return false; }
        if (conHijos) JuntarSubarbol(o, elegir); else elegir.push_back(o);
    }
    std::vector<Object*> antes;
    if (SceneCollection) JuntarSubarbol(SceneCollection, antes);
    InteractionMode = ObjectMode;
    estado = editNavegacion;
    DeseleccionarTodo();
    for (size_t i = 0; i < elegir.size(); i++) elegir[i]->Seleccionar();
    ObjActivo = elegir[0];
    if (modo == "vinculado") NewInstance(); else DuplicatedObject();
    estado = editNavegacion;   // los dos dejan agarrado el MOVER (como el teclado): se suelta
    std::vector<Object*> ahora;
    if (SceneCollection) JuntarSubarbol(SceneCollection, ahora);
    const int nuevos = (int)ahora.size() - (int)antes.size();
    std::string elegidas;
    for (size_t i = 0; i < ObjSelects.size(); i++) elegidas += (i ? ", " : "") + ObjSelects[i]->name;
    printf("      [dupjerarquia] %s de %d objeto(s): %d nuevo(s); elegidas: %s\n", modo.c_str(), (int)elegir.size(),
           nuevos, elegidas.empty() ? "-" : elegidas.c_str());
    if (nuevos <= 0) { err = "dupjerarquia: no se creo nada"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  clipactivo
// ---------------------------------------------------------------------------
bool CmdClipActivo(std::istringstream& ss, std::string& err) {
    std::string nom, clip; ss >> nom >> clip;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
    if (!o || o->getType() != ObjectType::armature || clip.empty()) { err = "clipactivo: uso: clipactivo <armature> <clip>"; return false; }
    Armature* a = (Armature*)o;
    for (size_t i = 0; i < a->animations.size(); i++)
        if (a->animations[i] && a->animations[i]->name == clip) {
            a->animActiva = (int)i;
            a->lastPoseFrame = -999999; a->lastPoseAnim = -999;
            printf("      [clipactivo] '%s' -> '%s' (%d)\n", nom.c_str(), clip.c_str(), (int)i);
            return true;
        }
    err = "clipactivo: '" + nom + "' no tiene un clip '" + clip + "'";
    return false;
}

// ---------------------------------------------------------------------------
//  objrel
// ---------------------------------------------------------------------------
const char* NombreTipo(Object* o) {
    switch (o->getType()) {
        case ObjectType::armature: return "armature";
        case ObjectType::mesh:     return "mesh";
        case ObjectType::hitbox:   return "hitbox";
        case ObjectType::instance: return "instance";
        case ObjectType::empty:    return "empty";
        default:                   return "otro";
    }
}

bool CmdObjRel(std::istringstream& ss, std::string& err) {
    std::string nom; ss >> nom;
    if (nom.empty()) { err = "objrel: uso: objrel <objeto> [nada] [padre P|-] [skin A|-] [hijos N] [tipo T]"; return false; }
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
    std::string k;
    if (!o) {
        printf("      [objrel] '%s': NO esta en la escena\n", nom.c_str());
        while (ss >> k) if (k != "nada") { err = "objrel: no hay un objeto '" + nom + "'"; return false; }
        return true;
    }
    const std::string padre = (o->Parent && o->Parent != SceneCollection) ? o->Parent->name : std::string("-");
    std::string skin = "-", skinMod = "-";
    if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        if (m->skinArmature) skin = m->skinArmature->name;
        for (size_t i = 0; i < m->modificadores.size(); i++)
            if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature && m->modificadores[i]->target)
                skinMod = m->modificadores[i]->target->name;
    }
    printf("      [objrel] '%s' tipo=%s padre=%s skin=%s (modificador %s) hijos=%d\n", nom.c_str(), NombreTipo(o),
           padre.c_str(), skin.c_str(), skinMod.c_str(), (int)o->Childrens.size());
    while (ss >> k) {
        std::string v;
        if (k == "nada") { err = "objrel: '" + nom + "' esta en la escena"; return false; }
        if (!(ss >> v)) { err = "objrel: falta el valor de '" + k + "'"; return false; }
        if (k == "padre") { if (padre != v) { err = "objrel: '" + nom + "' cuelga de '" + padre + "' (se esperaba '" + v + "')"; return false; } }
        else if (k == "skin") {
            if (skin != v || skinMod != v) { err = "objrel: '" + nom + "' se deforma con '" + skin + "' (modificador '" + skinMod + "'), se esperaba '" + v + "'"; return false; }
        }
        else if (k == "hijos") {
            if ((int)o->Childrens.size() != atoi(v.c_str())) {
                char b[160]; snprintf(b, sizeof(b), "objrel: '%s' tiene %d hijos (se esperaban %s)", nom.c_str(), (int)o->Childrens.size(), v.c_str());
                err = b; return false;
            }
        }
        else if (k == "tipo") { if (v != NombreTipo(o)) { err = "objrel: '" + nom + "' es " + NombreTipo(o) + " (se esperaba " + v + ")"; return false; } }
        else { err = "objrel: argumento desconocido '" + k + "'"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  personajesdirector
// ---------------------------------------------------------------------------
bool CmdPersonajesDirector(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "personajesdirector: uso: personajesdirector <carpeta>"; return false; }
    if (!SceneCollection || !FindObjectByName(SceneCollection, NombrePersonaje(1))) {
        err = "personajesdirector: falta la escena de 'personajesmin <carpeta> 2' (o mas)"; return false;
    }
    if (!Escribir(dir + "/pj_director.lua", kLuaDirector)) { err = "personajesdirector: no pude escribir el .lua en " + dir; return false; }
    Empty* d = new Empty(NULL, Vector3(0.0f, -12.0f, 0.0f));
    d->SetNameObj("Director");
    Script(d, "pj_director.lua");
    // el PASEANTE y su animacion de escena "Paseo" (x de -20 a -10 en 30 frames): la mezcla la
    // capa de escena que pide el Director. La escena activa del timeline no cambia.
    Empty* pas = new Empty(NULL, Vector3(-20.0f, 0.0f, -8.0f));
    pas->SetNameObj("Paseante");
    InitSceneAnimations();
    const int activa = SceneAnimActiva;
    const int idx = NuevaEscena();
    RenombrarEscenaActiva("Paseo");
    SceneAnimations[idx]->startFrame = 1; SceneAnimations[idx]->endFrame = 30; SceneAnimations[idx]->fps = 30;
    { AnimationObject ao; ao.obj = pas;
      AnimProperty& px = PropertyDeLista(ao.Propertys, AnimPosition, AnimX);
      SetKeyCurva(px, 1, -20.0f); SetKeyCurva(px, 30, -10.0f);
      ao.UpdateFirstLastFrame();
      AnimationObjects.push_back(ao); }
    SetEscenaActiva(activa);
    printf("      [personajesdirector] 'Director' con pj_director.lua + 'Paseante' con su escena 'Paseo' en '%s'\n", dir.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  esqjuego
// ---------------------------------------------------------------------------
bool CmdEsqJuego(std::istringstream& ss, std::string& err) {
    std::string nom; ss >> nom;
    if (nom.empty()) {
        err = "esqjuego: uso: esqjuego <armature>|escenas [clip C|-] [frame F] [vel V] [loop 0|1] [termino 0|1] [capas N] [trans 0|1]";
        return false;
    }
    std::string k;
    if (nom == "escenas") {
        printf("      [esqjuego] mix de escenas: %d capa(s)\n", (int)g_mixEscenas.size());
        while (ss >> k) {
            if (k != "capas") { err = "esqjuego escenas: argumento desconocido '" + k + "'"; return false; }
            int n = -1; ss >> n;
            if ((int)g_mixEscenas.size() != n) {
                char b[128]; snprintf(b, sizeof(b), "esqjuego: el mix de escenas tiene %d capa(s) (se esperaban %d)", (int)g_mixEscenas.size(), n);
                err = b; return false;
            }
        }
        return true;
    }
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
    if (!o || o->getType() != ObjectType::armature) { err = "esqjuego: no hay un armature '" + nom + "'"; return false; }
    Armature* a = (Armature*)o;
    const bool hayClip = a->animActiva >= 0 && a->animActiva < (int)a->animations.size() && a->animations[a->animActiva];
    const std::string clip = hayClip ? a->animations[a->animActiva]->name : std::string("-");
    const bool trans = a->transRestante > 0.0f;
    printf("      [esqjuego] '%s' clip=%s frame=%.3f vel=%.3f loop=%d termino=%d capas=%d trans=%.3f\n",
           nom.c_str(), clip.c_str(), a->juegoFrame, a->juegoVel, a->juegoLoop ? 1 : 0, a->juegoTermino ? 1 : 0,
           (int)a->capas.size(), a->transRestante);
    while (ss >> k) {
        char b[200]; b[0] = 0;
        if (k == "clip") {
            std::string c; ss >> c;
            if (c != clip) snprintf(b, sizeof(b), "el clip activo es '%s' (se esperaba '%s')", clip.c_str(), c.c_str());
        } else if (k == "frame" || k == "vel") {
            float v = -1.0f; ss >> v;
            const float real = (k == "frame") ? a->juegoFrame : a->juegoVel;
            if (fabsf(real - v) > 0.01f) snprintf(b, sizeof(b), "%s=%.3f (se esperaba %.3f)", k.c_str(), real, v);
        } else if (k == "loop" || k == "termino" || k == "trans") {
            int v = -1; ss >> v;
            const bool real = (k == "loop") ? a->juegoLoop : (k == "termino") ? a->juegoTermino : trans;
            if ((v != 0) != real) snprintf(b, sizeof(b), "%s=%d (se esperaba %d)", k.c_str(), real ? 1 : 0, v);
        } else if (k == "capas") {
            int n = -1; ss >> n;
            if ((int)a->capas.size() != n) snprintf(b, sizeof(b), "capas=%d (se esperaban %d)", (int)a->capas.size(), n);
        } else { err = "esqjuego: argumento desconocido '" + k + "'"; return false; }
        if (b[0]) { err = "esqjuego: '" + nom + "' " + b; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  mixescena
// ---------------------------------------------------------------------------
bool CmdMixEscena(std::istringstream& ss, std::string& err) {
    std::string sub, capa, nom; ss >> sub;
    if (sub == "mezclar") {
        int frame = 1; ss >> capa >> frame >> nom;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
        if (capa.empty() || !o) { err = "mixescena: uso: mixescena mezclar <capa> <frame> <objeto>"; return false; }
        bool esta = false;
        for (size_t k = 0; k < g_mixEscenas.size(); k++) if (g_mixEscenas[k].anim == capa) esta = true;
        if (!esta) { W3dCapaAnim c; c.anim = capa; g_mixEscenas.push_back(c); }
        const Vector3 antes = o->pos;
        W3dMixEscenasAplicar(frame, false);
        printf("      [mixescena] mezclar '%s' en %d: '%s' (%.3f,%.3f,%.3f) -> (%.3f,%.3f,%.3f)\n", capa.c_str(), frame,
               nom.c_str(), antes.x, antes.y, antes.z, o->pos.x, o->pos.y, o->pos.z);
        if (o->pos.x == antes.x && o->pos.y == antes.y && o->pos.z == antes.z) {
            err = "mixescena: la capa '" + capa + "' no movio a '" + nom + "' (no la toca?)"; return false;
        }
        return true;
    }
    if (sub == "soltar") {
        float x = 0, y = 0, z = 0; ss >> nom >> x >> y >> z;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
        if (!o) { err = "mixescena: uso: mixescena soltar <objeto> <x> <y> <z>"; return false; }
        W3dMixEscenasSoltar();
        g_mixEscenas.clear();
        printf("      [mixescena] soltar: '%s' en (%.3f,%.3f,%.3f)\n", nom.c_str(), o->pos.x, o->pos.y, o->pos.z);
        if (fabsf(o->pos.x - x) > 0.001f || fabsf(o->pos.y - y) > 0.001f || fabsf(o->pos.z - z) > 0.001f) {
            char b[200]; snprintf(b, sizeof(b), "mixescena: al soltar, '%s' quedo en (%.3f,%.3f,%.3f) y su base es (%.3f,%.3f,%.3f)",
                                  nom.c_str(), o->pos.x, o->pos.y, o->pos.z, x, y, z);
            err = b; return false;
        }
        return true;
    }
    err = "mixescena: uso: mixescena mezclar <capa> <frame> <objeto> | mixescena soltar <objeto> <x> <y> <z>";
    return false;
}

// ---------------------------------------------------------------------------
//  reporaiz
// ---------------------------------------------------------------------------
std::string RutaReal(const std::string& r) {
#ifdef _WIN32
    char b[4096];
    return _fullpath(b, r.c_str(), sizeof(b)) ? std::string(b) : r;
#else
    char b[4096];
    return realpath(r.c_str(), b) ? std::string(b) : r;
#endif
}
// pone (o saca, con v NULL) una variable de entorno del proceso
void PonerEntorno(const char* k, const char* v) {
#ifdef _WIN32
    _putenv_s(k, v ? v : "");
#else
    if (v) setenv(k, v, 1); else unsetenv(k);
#endif
}

bool CmdRepoRaiz(std::istringstream& ss, std::string& err) {
    std::string esperado; ss >> esperado;
    if (esperado.empty()) { err = "reporaiz: uso: reporaiz <esperado> [ajustes <ruta>|-] [entorno <ruta>|-] [via <camino>]"; return false; }
    bool ponerAjustes = false, ponerEntorno = false;
    std::string ajustes, entorno, via, k;
    while (ss >> k) {
        if (k == "ajustes")      { ss >> ajustes; ponerAjustes = true; if (ajustes == "-") ajustes.clear(); }
        else if (k == "entorno") { ss >> entorno; ponerEntorno = true; if (entorno == "-") entorno.clear(); }
        else if (k == "via")     ss >> via;
        else { err = "reporaiz: argumento desconocido '" + k + "'"; return false; }
    }
    // lo de ahora se guarda y se repone al salir (el resto de la corrida no se entera)
    const std::string ajustesAntes = cfg.repoPath;
    const char* e0 = getenv("W3D_REPO");
    const bool habiaEntorno = (e0 != NULL);
    const std::string entornoAntes = e0 ? e0 : "";
    if (ponerAjustes) cfg.repoPath = ajustes;
    if (ponerEntorno) PonerEntorno("W3D_REPO", entorno.empty() ? NULL : entorno.c_str());
    std::string comoFue;
    const std::string raiz = CompilarJuegoRepoRaiz(&comoFue);
    cfg.repoPath = ajustesAntes;
    if (ponerEntorno) PonerEntorno("W3D_REPO", habiaEntorno ? entornoAntes.c_str() : NULL);
    printf("      [reporaiz] '%s' (via %s)\n", raiz.c_str(), comoFue.empty() ? "-" : comoFue.c_str());
    if (raiz.empty()) { err = "reporaiz: no encontro la raiz del repo"; return false; }
    if (RutaReal(raiz) != RutaReal(esperado)) {
        err = "reporaiz: encontro '" + RutaReal(raiz) + "' y se esperaba '" + RutaReal(esperado) + "'"; return false;
    }
    if (!via.empty() && via != comoFue) { err = "reporaiz: la dio '" + comoFue + "' y se esperaba '" + via + "'"; return false; }
    return true;
}

} // namespace

// ============================================================================
//  el despachador
// ============================================================================
bool W3dPruebasIntegracionCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "personajesmin") return CmdPersonajesMin(ss, err);
    if (cmd == "personajesver") return CmdPersonajesVer(ss, err);
    if (cmd == "personajeslog") return CmdPersonajesLog(ss, err);
    if (cmd == "personajesdirector") return CmdPersonajesDirector(ss, err);
    if (cmd == "dupjerarquia")  return CmdDupJerarquia(ss, err);
    if (cmd == "objrel")        return CmdObjRel(ss, err);
    if (cmd == "clipactivo")    return CmdClipActivo(ss, err);
    if (cmd == "esqjuego")      return CmdEsqJuego(ss, err);
    if (cmd == "mixescena")     return CmdMixEscena(ss, err);
    if (cmd == "reporaiz")      return CmdRepoRaiz(ss, err);
    manejado = false;
    return false;
}
