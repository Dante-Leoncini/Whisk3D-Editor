// ============================================================================
//  W3dTestsPrefabs.cpp — comandos de harness de las INSTANCIAS DE PREFAB
//  (objects/InstancePrefab.h, io/Prefabs.h, io/PrefabsEditor.h).
//
//  Comandos:
//    prefabenemigo <carpeta> [fisica 0|1]
//        el juego 3D minimo (juego3dmin) + un PREFAB "Enemigo" armado por codigo: su objeto raiz
//        "Enemigo" (un vacio) con un "Esqueleto" (armature de 2 huesos con los clips "Saludo" y
//        "Quieto") que tiene colgados un "Cuerpo" (cilindro skinneado a el, recurso de malla
//        "Cuerpo") y un "Sensor" (hitbox, filtro "jugador"); el esqueleto lleva enemigo.lua (refs:
//        cuerpo = Cuerpo, vida = 10). Con 'fisica 1' el objeto raiz del prefab es un cubo "Caja" con
//        cuerpo rigido dinamico y caja.lua (arranca cayendo). Escribe los .lua (enemigo, caja y el
//        director de juegoprefabsmin) en la carpeta y vuelve a la escena del bloque.
//    prefabinstanciar <prefab> <x> <y> <z> [veces N] [nombre N]   una instancia en ese punto (W3dPrefabAdd,
//        la puerta de Add > Prefab y de soltarlo de la biblioteca: con Ctrl+Z)
//    prefabinfo <instancia> [prefab P] [generados N] [vacia 0|1] [overrides N] [raiz R]
//    prefabnodo <instancia> <ruta> [nada] [nombre N] [tipo T] [visible 0|1] [generado 0|1] [ref <prop> <valor|->]
//               [curvas N]
//        el objeto generado de esa ruta (relativa al objeto raiz generado: "." es el, como las de los
//        clips de jerarquia)
//    prefabscope <instancia> <ruta> <nombre> <esperado>
//        W3dFindNameFrom desde ese objeto generado: el hallado tiene que ser de la instancia
//        'esperado' ('-' = de la escena, fuera de toda instancia; 'nada' = no se encuentra)
//    prefabskin <instancia>
//        el Cuerpo de la instancia se deforma con el Esqueleto de la MISMA instancia (skinArmature y
//        el target de su modificador) y el Sensor avisa a ese esqueleto (su dueno)
//    prefabcompartido <prefab> [instancias N] [mallas N] [animsets N]
//        TODAS las instancias: los cuerpos usan EL MISMO recurso de malla (la misma memoria de
//        reposo) y los esqueletos los MISMOS clips en memoria (mismo animset). 'mallas'/'animsets' =
//        cuantos recursos distintos usan
//    prefabjer <instancia> <ruta> <clip> [loop 0|1]  reproduce un clip de jerarquia en ese objeto generado
//    prefabclip <instancia> <ruta> <clip>           el clip activo de un esqueleto generado
//    prefabpose <instA> <instB> <frame> distinta|igual
//        poses POR INSTANCIA como en el juego: el skinning de cada cuerpo con su esqueleto en ese frame
//    prefabregenerar <prefab> [n N]                  regenera todas sus instancias (todas las raices)
//    prefabcontar <prefab> <n>                       cuantas instancias tiene (los usuarios de la biblioteca)
//    prefabcrear [nombre N] [falla]                  "Create Prefab" desde la seleccion
//    prefabdesempaquetar <instancia> [falla]         "Unpack"
//    prefabcambiar <instancia> <prefab> [falla]      otro prefab (con Ctrl+Z)
//    prefabreset <instancia> [falla]                 "Reset Overrides" (con Ctrl+Z)
//    prefabh <instancia> <ruta>                      elegir un objeto generado y ocultarlo con H (dos pasos de
//                                                    undo que lo nombran por puntero)
//    prefaborigen <n>                                el "_origen" de un nodo en seco se lee exacto (sin float)
//    prefabkey                                       Insert Keyframe (la I) sobre la seleccion
//    prefabadd cube|empty|proxy                      el item del menu Add del viewport 3D
//    prefabmenu [n N] [tiene P]... [notiene P]...    el submenu Add > Prefab
//    prefabmenuelegir <prefab>                       elige esa fila del submenu (una instancia en el cursor 3D)
//    escenamenu [n N] [tiene E]... [notiene E]...   el submenu Add > Scene (las otras escenas, sin ciclos)
//    escenamenuelegir <escena>                       elige esa fila (una instancia de la ESCENA en el cursor 3D;
//                                                    prefabinstanciar escena:<nombre> x y z hace lo mismo en un punto)
//    prefaboverride <instancia> visible <ruta> 0|1   el ojo de un hijo generado
//    prefaboverride <instancia> prop <ruta> <prop> <valor>   el valor de la tarjeta Scripts de un generado
//    prefabvivos <n>                                 cuantas instancias hay en la raiz activa (todo el arbol)
//    prefabmundo <instancia> <ruta> <eje x|y|z> <menor|mayor> <valor>   posicion de MUNDO de un generado
//    prefabdef <prefab> [memoria 0|1]                su definicion (de la memoria del editor o de su entrada)
//    juegoprefabsmin <carpeta>
//        prefabenemigo + 3 instancias en la escena (la del medio con el override vida=25) + un
//        "Director" (director.lua) que en el Play instancia y destruye enemigos por lua (100 ciclos
//        de 10) y deja el veredicto en getShared("juez") y en el log ("[prefabs] OK ...")
//    juegoprefabslog <carpeta> <nombre>              corre el juego compilado y exige el mismo veredicto
//    prefabelegir <objeto> [objeto]...               elige esos objetos de la raiz activa (el ultimo queda activo)
//    prefabobj <objeto> [pos X Y Z] [euler X Y Z] [escala X Y Z] [indice N] [padre P|-] [luz 0|1] [camara 0|1]
//              [curvas N] [tipo T] [ref <prop> <valor|->]
//        un objeto de la raiz activa: su transform LOCAL (tol 1e-3), su lugar entre sus hermanos, de quien cuelga
//        ('-' = de la raiz), si es una luz de la raiz (Lights), si es la camara activa y cuantas curvas de las
//        animaciones de escena de la raiz lo nombran
//    prefabbandera
//        (en el proyecto de prefabenemigo) un prefab "Bandera": su objeto raiz es un plano "Bandera" (recurso
//        "BanderaMalla") con una VERTEX ANIM "ondear" de dos keyframes (1: reposo, 10: todo 1 mas arriba)
//    prefabframes <prefab> [instancias N] [juegos N] [plantilla 0|1]
//        las vertex anims de lo generado por TODAS sus instancias: cuantos juegos de frames distintos hay en
//        memoria ('juegos'); 'plantilla 1' = son los MISMOS que los de la raiz CARGADA del prefab
//    prefabvakey <prefab> <frame>                    un keyframe nuevo en la vertex anim de la PLANTILLA (su raiz
//        cargada, sin abrirla): la edicion hace la copia de sus frames (COW)
//    prefabvapose <instA> <instB>
//        la pose de la vertex anim es POR INSTANCIA aunque los frames sean compartidos: A en el frame 10 y B en
//        el 1 dan geometrias distintas (y B sigue en reposo)
//    prefabpila
//        (en el proyecto de prefabenemigo) un "Piso" con cuerpo ESTATICO en la escena activa y un prefab "Pila":
//        su objeto raiz "Pila" (vacio) -> "Base" (vacio en 1,1,0) -> "Bloque" (cubo con cuerpo rigido dinamico y
//        caja.lua): el cuerpo esta DOS niveles adentro de lo generado
//    prefabrigido <instancia> <ruta> [cuerpo 0|1] [semi S] [centro x|y|z menor|mayor V]
//        el cuerpo rigido runtime de un objeto generado (con el Play andando): si existe, su semieje X (la caja
//        medida con la escala de MUNDO, tol 1e-3) y el centro de la caja en el mundo
// ============================================================================
#include "test/W3dTestsPrefabs.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "objects/Objects.h"
#include "objects/InstancePrefab.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Armature.h"
#include "objects/Empty.h"
#include "objects/Hitbox.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"
#include "io/JsonW3d.h"                // prefaborigen: el lector de siempre
#include "undo/Undo.h"                 // prefabh: los pasos de seleccion y visibilidad
#include "io/EditorRoots.h"
#include "io/ProjectMeshes.h"          // W3dMeshCreateResource
#include "objects/ObjectMode.h"        // ReparentKeepTransform (la puerta de reparent)
#include "W3dRoots.h"
#include "animation/SkeletalAnimation.h"
#include "animation/Animation.h"
#include "animation/W3dAnimSet.h"
#include "animation/VertexAnimation.h" // prefabbandera / prefabframes / prefabvapose
#include "objects/Light.h"            // prefabobj: las luces de la raiz
#include "objects/Camera.h"           // prefabobj: la camara activa
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"          // WeightPaintEnsureMap
#include "physics/W3dRigid.h"
#include "script/W3dScript.h"
#include "base/W3dInteractionState.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "ViewPorts/ViewPort3D.h"      // MenuPrefabsAdd
#include "ViewPorts/LayoutInput.h"     // prefabadd: AddCube (el menu Add)
#include "w3dFilesystem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <set>

void AddEmpty();   // (LayoutInput.cpp: la accion de Add > Empty; prefabadd)
void AddProxyW3d();   // (LayoutInput.cpp: Add > Proxy W3D; prefabadd proxy)

namespace {

// ---------------------------------------------------------------------------
//  los .lua de la prueba
// ---------------------------------------------------------------------------
const char* kLuaEnemy =
    "-- un enemigo del prefab de prueba: cada instancia se maneja a si misma\n"
    "properties = { cuerpo = \"object\", vida = 10 }\n"
    "function start()\n"
    "  local mine = find(\"Cuerpo\", self())      -- el cuerpo de MI esqueleto\n"
    "  local seen = find(\"Cuerpo\")              -- sin raiz: por scope (primero MI instancia)\n"
    "  local ref = object(\"cuerpo\")             -- la referencia de la tarjeta: por scope tambien\n"
    "  if mine ~= nil and seen == mine and ref == mine then\n"
    "    setShared(\"scope_ok\", (getShared(\"scope_ok\") or 0) + 1)\n"
    "  end\n"
    "  setShared(\"enemigos\", (getShared(\"enemigos\") or 0) + 1)\n"
    "  setShared(\"vida_total\", (getShared(\"vida_total\") or 0) + property(\"vida\"))\n"
    "end\n"
    "function update(dt)\n"
    "  setShared(\"latidos\", (getShared(\"latidos\") or 0) + 1)\n"
    "end\n";
const char* kLuaBox =
    "-- la caja con fisica del prefab: arranca cayendo (los dinamicos nacen dormidos)\n"
    "function start() physicsVel(self(), 0, 0, -4) end   -- hacia abajo (Z arriba)\n";
// EL DIRECTOR: instancia y destruye enemigos EN JUEGO; al final, el veredicto
const char* kLuaDirector =
    "-- director de la prueba de prefabs: instanciar()/destruir() en juego, con un juez al final\n"
    "properties = { ciclos = 100, porCiclo = 10, escena = 3 }\n"
    "local live = {}\n"
    "local cycle, created, phase, wait, lat0 = 0, 0, 0, 0, 0\n"
    "local old = nil\n"
    "-- lat1/primer: los latidos entre este frame y el siguiente al primer instanciar(): lo creado corre su actualizar()\n"
    "-- desde el frame SIGUIENTE (y despues del director, que esta antes en el arbol), asi que solo cuentan los de\n"
    "-- la escena. hud: una escena UI no se destruye con destruir() (el juego compilado la seguia usando)\n"
    "local lat1, first, hud = 0, -1, false\n"
    "function update(dt)\n"
    "  if phase == 0 then\n"
    "    cycle = cycle + 1\n"
    "    if cycle == 1 then\n"
    "      lat1 = getShared(\"latidos\") or 0\n"
    "      hud = (find(\"HUD\") ~= nil) and not destroy(find(\"HUD\"))\n"
    "    elseif cycle == 2 then first = (getShared(\"latidos\") or 0) - lat1 end\n"
    "    for i = 1, #live do destroy(live[i]) end\n"
    "    if #live > 0 then old = live[1] end\n"
    "    live = {}\n"
    "    if cycle <= property(\"ciclos\") then\n"
    "      for i = 1, property(\"porCiclo\") do\n"
    "        local e = instantiate(\"Enemigo\", i * 2, -20, 0, 90)\n"
    "        if e then live[#live + 1] = e; created = created + 1 end\n"
    "      end\n"
    "    else phase = 1 end\n"
    "    return\n"
    "  end\n"
    "  if phase == 1 then lat0 = getShared(\"latidos\") or 0; phase = 2; wait = 0; return end\n"
    "  if phase == 2 then\n"
    "    wait = wait + 1\n"
    "    if wait < 5 then return end\n"
    "    local lat = (getShared(\"latidos\") or 0) - lat0\n"
    "    local esc = property(\"escena\")\n"
    "    local n = esc + created\n"
    "    local dead = (old ~= nil) and (getType(old) == \"\")\n"
    "    local ok = created == property(\"ciclos\") * property(\"porCiclo\") and (getShared(\"enemigos\") or 0) == n\n"
    "      and (getShared(\"scope_ok\") or 0) == n and lat == esc * 5 and dead and first == esc and hud\n"
    "    local line = string.format(\"[prefabs] %s creados=%d enemigos=%d scope=%d latidos=%d muerto=%s vida=%d primer=%d hud=%s\",\n"
    "      ok and \"OK\" or \"FALTA\", created, getShared(\"enemigos\") or -1, getShared(\"scope_ok\") or -1, lat,\n"
    "      tostring(dead), getShared(\"vida_total\") or -1, first, tostring(hud))\n"
    "    info(line)\n"
    "    setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    phase = 3\n"
    "    quit()\n"
    "  end\n"
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
std::string Ent(long n) { char b[32]; sprintf(b, "%ld", n); return b; }

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

// la instancia por nombre (las instancias son de la escena: su nombre es unico en el scope global)
InstancePrefab* Inst(const std::string& n, std::string& err, const char* who) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    // (un PROXY W3D de una libreria tambien: genera con la misma maquinaria)
    if (!o || !W3dIsInstanceType(o->getType())) { err = std::string(who) + ": no hay una instancia '" + n + "'"; return NULL; }
    return (InstancePrefab*)o;
}
// el objeto generado de una ruta (relativa al objeto raiz generado)
Object* FindNode(InstancePrefab* ip, const std::string& path) {
    Object* r = ip ? ip->RootGenerated() : NULL;
    return r ? W3dHierNode(r, path) : NULL;
}
int Count(Object* o) {
    int n = 0;
    for (size_t i = 0; i < o->Childrens.size(); i++) n += 1 + Count(o->Childrens[i]);
    return n;
}
void CollectInst(Object* o, std::vector<InstancePrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (W3dIsInstanceType(o->Childrens[i]->getType())) out.push_back((InstancePrefab*)o->Childrens[i]);
        CollectInst(o->Childrens[i], out);
    }
}
template <class T> T* ChildType(Object* o, ObjectType t) {
    if (!o) return NULL;
    if (o->getType() == t) return (T*)o;
    for (size_t i = 0; i < o->Childrens.size(); i++) { T* r = ChildType<T>(o->Childrens[i], t); if (r) return r; }
    return NULL;
}
// el valor de la propiedad 'prop' en el PRIMER script de 'o' que la tiene ("" + false = ninguno la tiene)
bool ValueRef(Object* o, const std::string& prop, std::string& out) {
    if (!o || !o->scriptData) return false;
    for (size_t s = 0; s < o->scriptData->scripts.size(); s++) {
        const std::vector<std::pair<std::string, std::string> >& r = o->scriptData->scripts[s].refs;
        for (size_t i = 0; i < r.size(); i++) if (r[i].first == prop) { out = r[i].second; return true; }
    }
    return false;
}
// 'ref <prop> <valor>' de prefabnodo / prefabobj ('-' = ningun script la tiene)
bool CheckRef(Object* o, std::istringstream& ss, const char* who, std::string& err) {
    std::string prop, expected; ss >> prop >> expected;
    std::string v;
    const bool has = ValueRef(o, prop, v);
    if (!has && expected == "-") return true;
    if (!has || v != expected) {
        err = std::string(who) + ": la ref '" + prop + "' de '" + o->name + "' es '" + (has ? v : std::string("(ninguna)")) + "', se esperaba '" + expected + "'";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  prefabenemigo
// ---------------------------------------------------------------------------
bool CmdPrefabEnemy(std::istringstream& ss, std::string& err) {
    std::string dir, k; int physics = 0;
    ss >> dir;
    while (ss >> k) { if (k == "physics") ss >> physics; }
    if (dir.empty()) { err = "prefabenemy: uso: prefabenemy <carpeta> [fisica 0|1]"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "prefabenemigo: " + e2; return false; }
    if (!Write(dir + "/enemigo.lua", kLuaEnemy) || !Write(dir + "/caja.lua", kLuaBox) ||
        !Write(dir + "/director.lua", kLuaDirector)) {
        err = "prefabenemy: no pude escribir los .lua en " + dir; return false;
    }
    // el PREFAB: su raiz (aislada) con el objeto raiz que genera
    const int idx = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Enemigo");
    if (idx < 0 || SceneCollection->Childrens.empty()) { err = "prefabenemy: no se pudo crear el prefab"; return false; }
    Object* root = SceneCollection->Childrens[0];
    Armature* a = new Armature(root, Vector3(0, 0, 0));
    a->SetNameObj("Esqueleto");
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0, 0, 0); r.tail = Vector3(0, 1, 0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0, 1, 0); c.tail = Vector3(0, 2, 0); a->bones.push_back(c); }
    PrepareAuthoredSkin(a);
    a->animations.push_back(Clip("Saludo", 1, AnimZ, 0.0f, 60.0f, 20));
    a->animations.push_back(Clip("Quieto", 0, AnimY, 0.0f, 35.0f, 30));
    a->activeAnim = 0;
    Mesh* s = (Mesh*)NewMesh(MeshType(MeshType::cylinder), a, false);
    if (!s) { err = "prefabenemy: NewMesh fallo"; return false; }
    s->SetNameObj("Cuerpo");
    s->pos = Vector3(0, 0, 0);
    s->GenerateRender();
    WeightPaintEnsureMap(s);
    VertexGroup* vr = new VertexGroup("raiz");
    VertexGroup* vb = new VertexGroup("brazo");
    std::vector<char> done(s->vertexSize, 0);
    for (int i = 0; i < s->vertexSize; i++) {
        const int cp = s->vertCtrlPoint[i];
        if (cp < 0 || cp >= (int)done.size() || done[cp]) continue;
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
    if (!W3dMeshCreateResource(s, "Cuerpo")) { err = "prefabenemy: no pude crear el recurso 'Cuerpo'"; return false; }
    Hitbox* h = HitboxCreateFitted(a);
    if (!h) { err = "prefabenemy: HitboxCrearAjustado fallo"; return false; }
    h->SetNameObj("Sensor");
    h->filter = "jugador";
    Script(a, "enemigo.lua"); Ref(a, "cuerpo", "Cuerpo"); Ref(a, "vida", "10");
    int nPrefabs = 1;
    if (physics) {
        // un SEGUNDO prefab "Caja": su objeto raiz ES un cubo con cuerpo rigido dinamico
        const int ic = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Caja");
        if (ic < 0) { err = "prefabenemy: no se pudo crear el prefab Caja"; return false; }
        Object* empty = SceneCollection->Childrens.empty() ? NULL : SceneCollection->Childrens[0];
        Mesh* cube = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
        cube->SetNameObj("Caja");
        W3dMeshCreateResource(cube, "CajaMalla");
        W3dRigidDef* d = new W3dRigidDef();
        d->type = 1; d->mass = 1.0f;
        for (int q = 0; q < 3; q++) { d->box[q] = 2.0f; d->center[q] = 0.0f; }
        cube->physics = d;
        Script(cube, "caja.lua");
        // el cubo reemplaza al vacio automatico como objeto raiz
        if (empty) {
            std::vector<Object*>& ch = SceneCollection->Childrens;
            for (size_t i = 0; i < ch.size(); i++) if (ch[i] == empty) { ch.erase(ch.begin() + (long)i); break; }
            DeselectAll(); ActiveObj = NULL;
            W3dFreeSubtree(empty);
        }
        cube->SetNameObj("Caja");   // (el nombre del vacio ya esta libre)
        nPrefabs = 2;
    }
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "prefabenemigo: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    printf("      [prefabenemigo] juego3dmin + %d prefab(s) (Enemigo: Esqueleto + Cuerpo skinneado + Sensor + enemigo.lua%s) en '%s'\n",
           nPrefabs, physics ? "; Caja: cubo con fisica" : "", dir.c_str());
    return true;
}

bool CmdPrefabInstantiate(std::istringstream& ss, std::string& err) {
    std::string p, k, expected; float x = 0, y = 0, z = 0; int times = 1;
    ss >> p >> x >> y >> z;
    while (ss >> k) { if (k == "name") ss >> expected; else if (k == "times") ss >> times; }
    std::string reason;
    InstancePrefab* ip = NULL;
    // 'veces N': N instancias en fila (x, x+2, x+4...); 'nombre' se compara con la ultima
    for (int i = 0; i < (times < 1 ? 1 : times); i++) {
        ip = W3dPrefabAdd(p, Vector3(x + 2.0f * (float)i, y, z), &reason);
        if (!ip) break;
    }
    printf("      [prefabinstanciar] %s x%d en (%g, %g, %g): %s %s\n", p.c_str(), times, x, y, z, ip ? ip->name.c_str() : "NO", reason.c_str());
    if (!ip) { err = "prefabinstanciar: " + reason; return false; }
    if (!expected.empty() && ip->name != expected) { err = "prefabinstantiate: se llamo '" + ip->name + "' y se esperaba '" + expected + "'"; return false; }
    return true;
}

bool CmdPrefabInfo(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    InstancePrefab* ip = Inst(n, err, "prefabinfo"); if (!ip) return false;
    const int gen = Count(ip);
    Object* r = ip->RootGenerated();
    W3dPrefabSyncOverrides(ip);
    const int ov = (int)(ip->overProps.size() + ip->overVisible.size());
    printf("      [prefabinfo] '%s': prefab '%s' generados=%d raiz=%s vacia=%d version=%u overrides=%d (props %d, visible %d)\n",
           ip->name.c_str(), ip->prefab.c_str(), gen, r ? r->name.c_str() : "-", ip->notGenerated ? 1 : 0, ip->generatedVersion,
           ov, (int)ip->overProps.size(), (int)ip->overVisible.size());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "prefabinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "prefab" && ip->prefab != v) { err = "prefabinfo: el prefab es '" + ip->prefab + "'"; return false; }
        if (k == "generated" && gen != atoi(v.c_str())) { err = "prefabinfo: genero " + Ent(gen) + " objetos, se esperaban " + v; return false; }
        if (k == "empty" && (ip->notGenerated ? 1 : 0) != atoi(v.c_str())) { err = "prefabinfo: vacia=" + Ent(ip->notGenerated ? 1 : 0); return false; }
        if (k == "overrides" && ov != atoi(v.c_str())) { err = "prefabinfo: tiene " + Ent(ov) + " overrides, se esperaban " + v; return false; }
        if (k == "root" && (!r || r->name != v)) { err = "prefabinfo: el objeto raiz generado no es '" + v + "'"; return false; }
        if (k != "prefab" && k != "generated" && k != "empty" && k != "overrides" && k != "root") { err = "prefabinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

int CountCurves(Object* o);   // (mas abajo: las curvas de las animaciones de escena que lo nombran)
bool CmdPrefabNode(std::istringstream& ss, std::string& err) {
    std::string n, path; ss >> n >> path;
    InstancePrefab* ip = Inst(n, err, "prefabnode"); if (!ip) return false;
    Object* o = FindNode(ip, path);
    printf("      [prefabnodo] %s:%s -> %s (%s) visible=%d generado=%d\n", n.c_str(), path.c_str(), o ? o->name.c_str() : "none",
           o ? W3dTypeName(o->getType()) : "-", o ? (int)o->visible : -1, o ? (int)W3dIsGenerated(o) : -1);
    std::string k;
    while (ss >> k) {
        if (k == "none") { if (o) { err = "prefabnode: la ruta existe"; return false; } continue; }
        if (!o) { err = "prefabnode: no hay nada en '" + path + "'"; return false; }
        if (k == "ref") { if (!CheckRef(o, ss, "prefabnode", err)) return false; continue; }
        std::string v; if (!(ss >> v)) { err = "prefabnode: falta el valor de '" + k + "'"; return false; }
        if (k == "name" && o->name != v) { err = "prefabnode: se llama '" + o->name + "'"; return false; }
        if (k == "type" && std::string(W3dTypeName(o->getType())) != v) { err = "prefabnode: es un '" + std::string(W3dTypeName(o->getType())) + "'"; return false; }
        if (k == "visible" && (int)o->visible != atoi(v.c_str())) { err = "prefabnode: visible=" + Ent(o->visible); return false; }
        if (k == "generated" && (int)W3dIsGenerated(o) != atoi(v.c_str())) { err = "prefabnode: generado=" + Ent(W3dIsGenerated(o)); return false; }
        if (k == "curves" && CountCurves(o) != atoi(v.c_str())) { err = "prefabnode: lo nombran " + Ent(CountCurves(o)) + " curvas"; return false; }
    }
    return true;
}

bool CmdPrefabScope(std::istringstream& ss, std::string& err) {
    std::string n, path, name, expected; ss >> n >> path >> name >> expected;
    InstancePrefab* ip = Inst(n, err, "prefabscope"); if (!ip) return false;
    Object* from = FindNode(ip, path);
    if (!from) { err = "prefabscope: no hay nada en '" + path + "'"; return false; }
    Object* o = W3dFindNameFrom(from, name);
    Object* of = o ? W3dInstanceOf(o) : NULL;
    const std::string sOf = !o ? std::string("none") : (of ? of->name : std::string("-"));
    printf("      [prefabscope] desde %s:%s busca '%s' -> %s (de %s)\n", n.c_str(), path.c_str(), name.c_str(), o ? o->name.c_str() : "none", sOf.c_str());
    if (sOf != expected) { err = "prefabscope: se encontro el de '" + sOf + "' y se esperaba '" + expected + "'"; return false; }
    return true;
}

bool CmdPrefabSkin(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    InstancePrefab* ip = Inst(n, err, "prefabskin"); if (!ip) return false;
    Armature* a = ChildType<Armature>(ip, ObjectType::armature);
    Mesh* m = ChildType<Mesh>(ip, ObjectType::mesh);
    Hitbox* h = ChildType<Hitbox>(ip, ObjectType::hitbox);
    if (!a || !m || !h) { err = "prefabskin: la instancia no genero esqueleto, cuerpo y sensor"; return false; }
    Object* tgt = NULL;
    for (size_t i = 0; i < m->modifiers.size(); i++)
        if (m->modifiers[i] && m->modifiers[i]->type == ModifierType::Armature) tgt = m->modifiers[i]->target;
    printf("      [prefabskin] '%s': cuerpo skin=%p mod=%p esqueleto=%p sensor duenio=%p\n", n.c_str(), (void*)m->skinArmature,
           (void*)tgt, (void*)a, (void*)W3dHitboxOwner(h));
    if ((Object*)m->skinArmature != (Object*)a || tgt != (Object*)a) { err = "prefabskin: el cuerpo no se deforma con el esqueleto de SU instancia"; return false; }
    if (W3dHitboxOwner(h) != (Object*)a) { err = "prefabskin: el sensor no avisa al esqueleto de SU instancia"; return false; }
    return true;
}

bool CmdPrefabShared(std::istringstream& ss, std::string& err) {
    std::string p, k; ss >> p;
    std::vector<InstancePrefab*> v;
    W3dPrefabInstances(p, v);
    std::set<const void*> recs, sets, verts;
    for (size_t i = 0; i < v.size(); i++) {
        Mesh* m = ChildType<Mesh>(v[i], ObjectType::mesh);
        Armature* a = ChildType<Armature>(v[i], ObjectType::armature);
        if (m) { recs.insert(m->mesh); verts.insert(m->vertex); if (m->mesh && m->vertex != m->mesh->vertex) { err = "prefabshared: la geometria de un cuerpo no es la de su recurso"; return false; } }
        if (a) {
            sets.insert(W3dArmatureAnimSetResource(a));
            if (!W3dArmatureAnimsShared(a)) { err = "prefabshared: el esqueleto de '" + v[i]->name + "' tiene clips propios"; return false; }
        }
    }
    printf("      [prefabcompartido] %s: %d instancia(s), %d recurso(s) de malla, %d geometria(s), %d animset(s)\n", p.c_str(),
           (int)v.size(), (int)recs.size(), (int)verts.size(), (int)sets.size());
    while (ss >> k) {
        int val = -1; if (!(ss >> val)) { err = "prefabshared: falta el valor de '" + k + "'"; return false; }
        if (k == "instances" && (int)v.size() != val) { err = "prefabshared: hay " + Ent((long)v.size()) + " instancias"; return false; }
        if (k == "meshes" && ((int)recs.size() != val || (int)verts.size() != val)) { err = "prefabcompartido: " + Ent((long)recs.size()) + " recursos de malla / " + Ent((long)verts.size()) + " geometrias"; return false; }
        if (k == "animsets" && (int)sets.size() != val) { err = "prefabcompartido: " + Ent((long)sets.size()) + " animsets"; return false; }
    }
    return true;
}

bool CmdPrefabClip(std::istringstream& ss, std::string& err) {
    std::string n, path, clip; ss >> n >> path >> clip;
    InstancePrefab* ip = Inst(n, err, "prefabclip"); if (!ip) return false;
    Object* o = FindNode(ip, path);
    if (!o || o->getType() != ObjectType::armature) { err = "prefabclip: '" + path + "' no es un esqueleto"; return false; }
    Armature* a = (Armature*)o;
    for (size_t i = 0; i < a->animations.size(); i++)
        if (a->animations[i] && a->animations[i]->name == clip) { a->activeAnim = (int)i; return true; }
    err = "prefabclip: el esqueleto no tiene el clip '" + clip + "'";
    return false;
}

bool CmdPrefabPose(std::istringstream& ss, std::string& err) {
    std::string a1, a2, mode; int frame = 10; ss >> a1 >> a2 >> frame >> mode;
    InstancePrefab* i1 = Inst(a1, err, "prefabpose"); if (!i1) return false;
    InstancePrefab* i2 = Inst(a2, err, "prefabpose"); if (!i2) return false;
    Armature* ar[2] = { ChildType<Armature>(i1, ObjectType::armature), ChildType<Armature>(i2, ObjectType::armature) };
    Mesh* me[2] = { ChildType<Mesh>(i1, ObjectType::mesh), ChildType<Mesh>(i2, ObjectType::mesh) };
    if (!ar[0] || !ar[1] || !me[0] || !me[1]) { err = "prefabpose: faltan esqueletos/cuerpos"; return false; }
    const int kind = ActiveAnimKind;
    ActiveAnimKind = 2;   // como en el juego: cada esqueleto con SU clip y SU cabezal
    for (int k = 0; k < 2; k++) {
        Armature* a = ar[k];
        if (a->activeAnim < 0 || a->activeAnim >= (int)a->animations.size()) { ActiveAnimKind = kind; err = "prefabpose: esqueleto sin clip activo"; return false; }
        a->gameFrame = (float)(frame - a->animations[a->activeAnim]->startFrame);
        EvaluatePoseSkeleton(a, frame);
        me[k]->lastSkinFrame = -999999;
        SkinMesh(me[k]);
    }
    ActiveAnimKind = kind;
    if (!me[0]->skinVertex || !me[1]->skinVertex) { err = "prefabpose: el skinning no produjo pose"; return false; }
    if (me[0]->skinVertex == me[1]->skinVertex) { err = "prefabpose: los dos cuerpos comparten el buffer de pose"; return false; }
    double d = 0.0;
    for (int i = 0; i < me[0]->vertexSize * 3 && i < me[1]->vertexSize * 3; i++) d += fabs(me[0]->skinVertex[i] - me[1]->skinVertex[i]);
    printf("      [prefabpose] %s vs %s en el frame %d: diferencia %.4f (%s)\n", a1.c_str(), a2.c_str(), frame, d, mode.c_str());
    if (mode == "distinct" && d < 1e-3) { err = "prefabpose: las poses son iguales (se esperaban distintas)"; return false; }
    if (mode == "equal" && d > 1e-5) { err = "prefabpose: las poses difieren (se esperaban iguales)"; return false; }
    return true;
}

bool CmdPrefabRegenerate(std::istringstream& ss, std::string& err) {
    std::string p, k; int expected = -1; ss >> p;
    while (ss >> k) if (k == "n") ss >> expected;
    W3dPrefabInvalidate(p);
    const int n = W3dPrefabRegenerateInstances(p);
    printf("      [prefabregenerar] %s: %d instancia(s)\n", p.c_str(), n);
    if (expected >= 0 && n != expected) { err = "prefabregenerate: regenero " + Ent(n) + ", se esperaban " + Ent(expected); return false; }
    return true;
}

bool CmdPrefabCount(std::istringstream& ss, std::string& err) {
    std::string p; int expected = -1; ss >> p >> expected;
    const int n = W3dPrefabCountInstances(p);
    printf("      [prefabcontar] %s: %d instancia(s)\n", p.c_str(), n);
    if (n != expected) { err = "prefabcount: hay " + Ent(n) + ", se esperaban " + Ent(expected); return false; }
    return true;
}

bool CmdPrefabCreate(std::istringstream& ss, std::string& err) {
    std::string k, expected; bool fault = false;
    while (ss >> k) { if (k == "name") ss >> expected; else if (k == "fails") fault = true; }
    std::string name, reason;
    InteractionMode = ObjectMode; state = editNavigation;
    const bool ok = W3dPrefabCreateFromSelection(&name, &reason);
    printf("      [prefabcrear] %s %s\n", ok ? name.c_str() : "NO", reason.c_str());
    if (ok == fault) { err = ok ? "prefabcreate: se creo y no debia" : "prefabcrear: " + reason; return false; }
    if (ok && !expected.empty() && name != expected) { err = "prefabcreate: el prefab se llamo '" + name + "'"; return false; }
    return true;
}

bool CmdPrefabUnpack(std::istringstream& ss, std::string& err) {
    std::string n, k; bool fault = false; ss >> n;
    while (ss >> k) if (k == "fails") fault = true;
    InstancePrefab* ip = Inst(n, err, "prefabunpack"); if (!ip) return false;
    std::string reason;
    InteractionMode = ObjectMode; state = editNavigation;
    const bool ok = W3dPrefabUnpack(ip, &reason);
    printf("      [prefabdesempaquetar] %s: %s %s\n", n.c_str(), ok ? "ok" : "NO", reason.c_str());
    if (ok == fault) { err = ok ? "prefabunpack: se desempaqueto y no debia" : "prefabdesempaquetar: " + reason; return false; }
    return true;
}

bool CmdPrefabChange(std::istringstream& ss, std::string& err) {
    std::string n, p, k; bool fault = false; ss >> n >> p;
    while (ss >> k) if (k == "fails") fault = true;
    InstancePrefab* ip = Inst(n, err, "prefabchange"); if (!ip) return false;
    std::string reason;
    const bool ok = W3dPrefabChange(ip, p, &reason);
    printf("      [prefabcambiar] %s -> %s: %s %s\n", n.c_str(), p.c_str(), ok ? "ok" : "NO", reason.c_str());
    if (ok == fault) { err = ok ? "prefabchange: cambio y no debia" : "prefabcambiar: " + reason; return false; }
    return true;
}

// prefabreset <instancia> [falla]: "Reset Overrides" de la tarjeta (con Ctrl+Z)
bool CmdPrefabReset(std::istringstream& ss, std::string& err) {
    std::string n, k; bool fault = false; ss >> n;
    while (ss >> k) if (k == "fails") fault = true;
    InstancePrefab* ip = Inst(n, err, "prefabreset"); if (!ip) return false;
    std::string reason;
    const bool ok = W3dPrefabResetOverrides(ip, &reason);
    printf("      [prefabreset] %s: %s %s\n", n.c_str(), ok ? "ok" : "NO", reason.c_str());
    if (ok == fault) { err = ok ? "prefabreset: reseteo y no debia" : "prefabreset: " + reason; return false; }
    return true;
}

// prefabh <instancia> <ruta>: el usuario ELIGE ese objeto generado (un click: su paso de undo de seleccion) y lo
// oculta con H (su paso de undo de visibilidad). Deja en el historial DOS pasos que lo nombran por puntero
bool CmdPrefabH(std::istringstream& ss, std::string& err) {
    std::string n, path; ss >> n >> path;
    InstancePrefab* ip = Inst(n, err, "prefabh"); if (!ip) return false;
    Object* o = FindNode(ip, path);
    if (!o) { err = "prefabh: no hay nada en '" + path + "'"; return false; }
    InteractionMode = ObjectMode; state = editNavigation;
    UndoCaptureSelection();
    DeselectAll(); o->Select(); ActiveObj = o;
    UndoCaptureVisibility();
    ChangeVisibilityObj();
    printf("      [prefabh] %s:%s visible=%d\n", n.c_str(), path.c_str(), (int)o->visible);
    return true;
}

// prefabadd cube|empty: el item del menu Add del viewport 3D (la accion de verdad: editando un prefab, lo nuevo va
// adentro de su objeto raiz)
bool CmdPrefabAdd(std::istringstream& ss, std::string& err) {
    std::string t; ss >> t;
    InteractionMode = ObjectMode; state = editNavigation;
    if (t == "cube") AddCube();
    else if (t == "empty") AddEmpty();
    else if (t == "proxy") AddProxyW3d();   // (Add > Proxy W3D)
    else { err = "prefabadd: uso: prefabadd cube|empty|proxy"; return false; }
    printf("      [prefabadd] %s -> '%s' (padre %s)\n", t.c_str(), ActiveObj ? ActiveObj->name.c_str() : "-",
           ActiveObj && ActiveObj->Parent && ActiveObj->Parent != SceneCollection ? ActiveObj->Parent->name.c_str() : "-");
    return true;
}

// prefabkey: Insert Keyframe (la I) sobre la seleccion, como el menu
bool CmdPrefabKey(std::istringstream& ss, std::string& err) {
    (void)ss; (void)err;
    InteractionMode = ObjectMode; state = editNavigation;
    InsertKeyframeObject(0);
    printf("      [prefabkey] %d elegido(s)\n", (int)ObjSelects.size());
    return true;
}

// prefaborigen <n>: el "_origen" (el serial de la plantilla) de un nodo en seco se lee EXACTO (entero sin signo,
// no por float: desde 2^24 se corria al vecino)
bool CmdPrefabOrigin(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    const std::string json = "{\"_origin\": " + n + "}";
    JParser p(json.data(), json.size());
    JVal* v = p.Value();
    const unsigned read = (v && !p.error && v->type == 4) ? JU(v, "_origin", 0u) : 0u;
    delete v;
    const unsigned expected = (unsigned)strtoul(n.c_str(), NULL, 10);
    printf("      [prefaborigen] %s -> %u\n", n.c_str(), read);
    if (read != expected) { err = "prefaborigin: se leyo " + Ent((long)read) + ", se esperaba " + n; return false; }
    return true;
}

bool CmdPrefabMenu(std::istringstream& ss, std::string& err) {
    PopupMenu m;
    W3dPrefabMenuAddBuild(&m);
    std::vector<std::string> names;
    for (size_t i = 0; i < m.items.size(); i++) if (m.items[i]->id > 0) names.push_back(m.items[i]->text);
    printf("      [prefabmenu] %d prefab(s):", (int)names.size());
    for (size_t i = 0; i < names.size(); i++) printf(" %s", names[i].c_str());
    printf("\n");
    std::string k, v;
    while (ss >> k >> v) {
        bool is = false;
        for (size_t i = 0; i < names.size(); i++) if (names[i] == v) is = true;
        if (k == "n" && (int)names.size() != atoi(v.c_str())) { err = "prefabmenu: tiene " + Ent((long)names.size()) + " filas"; return false; }
        if (k == "has" && !is) { err = "prefabmenu: no ofrece '" + v + "'"; return false; }
        if (k == "hasnot" && is) { err = "prefabmenu: ofrece '" + v + "'"; return false; }
    }
    return true;
}

// escenamenu [n N] [tiene E]... [notiene E]...   el submenu Add > Scene (las otras escenas, sin ciclos)
bool CmdSceneMenu(std::istringstream& ss, std::string& err) {
    PopupMenu m;
    W3dSceneMenuAddBuild(&m);
    std::vector<std::string> names;
    for (size_t i = 0; i < m.items.size(); i++) if (m.items[i]->id > 0) names.push_back(m.items[i]->text);
    printf("      [escenamenu] %d escena(s):", (int)names.size());
    for (size_t i = 0; i < names.size(); i++) printf(" %s", names[i].c_str());
    printf("\n");
    std::string k, v;
    while (ss >> k >> v) {
        bool is = false;
        for (size_t i = 0; i < names.size(); i++) if (names[i] == v) is = true;
        if (k == "n" && (int)names.size() != atoi(v.c_str())) { err = "scenemenu: tiene " + Ent((long)names.size()) + " filas"; return false; }
        if (k == "has" && !is) { err = "scenemenu: no ofrece '" + v + "'"; return false; }
        if (k == "hasnot" && is) { err = "scenemenu: ofrece '" + v + "'"; return false; }
    }
    return true;
}
// escenamenuelegir <escena>   elige esa fila de Add > Scene (una instancia de la escena en el cursor 3D)
bool CmdSceneMenuPick(std::istringstream& ss, std::string& err) {
    std::string p; ss >> p;
    if (!MenuScenesAdd) { err = "scenemenupick: no hay menu Add (sin viewport 3D)"; return false; }
    W3dSceneMenuAddBuild(MenuScenesAdd);
    for (size_t i = 0; i < MenuScenesAdd->items.size(); i++)
        if (MenuScenesAdd->items[i]->text == p && MenuScenesAdd->items[i]->id > 0) {
            InteractionMode = ObjectMode; state = editNavigation;
            MenuScenesAdd->Run(MenuScenesAdd->items[i]->id);
            printf("      [escenamenuelegir] %s -> activo '%s'\n", p.c_str(), ActiveObj ? ActiveObj->name.c_str() : "-");
            if (!ActiveObj || !W3dIsInstanceType(ActiveObj->getType())) { err = "scenemenupick: no se creo la instancia"; return false; }
            return true;
        }
    err = "scenemenupick: el menu no ofrece '" + p + "'";
    return false;
}

bool CmdPrefabMenuPick(std::istringstream& ss, std::string& err) {
    std::string p; ss >> p;
    if (!MenuPrefabsAdd) { err = "prefabmenupick: no hay menu Add (sin viewport 3D)"; return false; }
    W3dPrefabMenuAddBuild(MenuPrefabsAdd);
    for (size_t i = 0; i < MenuPrefabsAdd->items.size(); i++)
        if (MenuPrefabsAdd->items[i]->text == p) {
            InteractionMode = ObjectMode; state = editNavigation;
            MenuPrefabsAdd->Run(MenuPrefabsAdd->items[i]->id);
            printf("      [prefabmenuelegir] %s -> activo '%s'\n", p.c_str(), ActiveObj ? ActiveObj->name.c_str() : "-");
            // (un prefab de una LIBRERIA vinculada crea un PROXY)
            if (!ActiveObj || !W3dIsInstanceType(ActiveObj->getType())) { err = "prefabmenupick: no se creo la instancia"; return false; }
            return true;
        }
    err = "prefabmenupick: el submenu no ofrece '" + p + "'";
    return false;
}

bool CmdPrefabOverride(std::istringstream& ss, std::string& err) {
    std::string n, that; ss >> n >> that;
    InstancePrefab* ip = Inst(n, err, "prefaboverride"); if (!ip) return false;
    if (that == "visible") {
        std::string path; int v = 1; ss >> path >> v;
        Object* o = FindNode(ip, path);
        if (!o) { err = "prefaboverride: no hay nada en '" + path + "'"; return false; }
        o->visible = (v != 0);
    } else if (that == "prop") {
        std::string path, prop, value; ss >> path >> prop >> value;
        Object* o = FindNode(ip, path);
        if (!o || !o->scriptData || o->scriptData->scripts.empty()) { err = "prefaboverride: '" + path + "' no tiene scripts"; return false; }
        // como la tarjeta Scripts: escribe el valor en SU script (la instancia lo nota al sincronizar)
        std::vector<std::pair<std::string, std::string> >& refs = o->scriptData->scripts[0].refs;
        bool placed = false;
        for (size_t r = 0; r < refs.size(); r++) if (refs[r].first == prop) { refs[r].second = value; placed = true; }
        if (!placed) refs.push_back(std::make_pair(prop, value));
    } else { err = "prefaboverride: visible <ruta> 0|1 | prop <ruta> <prop> <valor>"; return false; }
    W3dPrefabSyncOverrides(ip);
    printf("      [prefaboverride] %s: %d prop(s), %d visible(s)\n", n.c_str(), (int)ip->overProps.size(), (int)ip->overVisible.size());
    return true;
}

bool CmdPrefabLive(std::istringstream& ss, std::string& err) {
    int expected = -1; ss >> expected;
    std::vector<InstancePrefab*> v;
    CollectInst(SceneCollection, v);
    printf("      [prefabvivos] %d instancia(s) en la raiz activa\n", (int)v.size());
    if (expected >= 0 && (int)v.size() != expected) { err = "prefablive: hay " + Ent((long)v.size()) + ", se esperaban " + Ent(expected); return false; }
    return true;
}

bool CmdPrefabWorld(std::istringstream& ss, std::string& err) {
    std::string n, path, axis, op; float value = 0; ss >> n >> path >> axis >> op >> value;
    InstancePrefab* ip = Inst(n, err, "prefabworld"); if (!ip) return false;
    Object* o = FindNode(ip, path);
    if (!o) { err = "prefabworld: no hay nada en '" + path + "'"; return false; }
    const Vector3 w = o->GetGlobalPosition();
    const float c = (axis == "x") ? w.x : (axis == "y") ? w.y : w.z;
    printf("      [prefabmundo] %s:%s en el mundo (%.3f, %.3f, %.3f)\n", n.c_str(), path.c_str(), w.x, w.y, w.z);
    if (op == "less" && !(c < value)) { err = "prefabmundo: " + axis + " no es menor"; return false; }
    if (op == "greater" && !(c > value)) { err = "prefabmundo: " + axis + " no es mayor"; return false; }
    return true;
}

// prefabjer <instancia> <ruta> <clip> [loop 0|1]: reproduce un CLIP DE JERARQUIA en ese objeto generado (el
// clip vino con el prefab: sus rutas usan los nombres del prefab, que la instancia genera tal cual)
bool CmdPrefabHier(std::istringstream& ss, std::string& err) {
    std::string n, path, clip, k; int loop = 1; ss >> n >> path >> clip;
    while (ss >> k) { if (k == "loop") ss >> loop; else { err = "prefabhier: no entiendo '" + k + "'"; return false; } }
    InstancePrefab* ip = Inst(n, err, "prefabhier"); if (!ip) return false;
    Object* o = FindNode(ip, path);
    if (!o) { err = "prefabhier: no hay nada en '" + path + "'"; return false; }
    W3dClipsViewsSync();
    const float d = W3dAnimObjectPlay(o, clip, loop != 0);
    printf("      [prefabjer] %s:%s '%s' loop=%d -> %.4f s\n", n.c_str(), path.c_str(), clip.c_str(), loop, d);
    if (d < 0.0f) { err = "prefabhier: '" + path + "' de '" + n + "' no tiene el clip '" + clip + "'"; return false; }
    return true;
}

// prefabsel <instancia> <ruta>: elige SOLO ese objeto generado (como un click en el outliner)
bool CmdPrefabSel(std::istringstream& ss, std::string& err) {
    std::string n, path; ss >> n >> path;
    InstancePrefab* ip = Inst(n, err, "prefabsel"); if (!ip) return false;
    Object* o = path.empty() ? (Object*)ip : FindNode(ip, path);
    if (!o) { err = "prefabsel: no hay nada en '" + path + "'"; return false; }
    InteractionMode = ObjectMode; state = editNavigation;
    DeselectAll(); o->Select(); ActiveObj = o;
    return true;
}
// prefabreparent <instancia> <ruta> <newParent> [falla]: la puerta de reparent de siempre (Ctrl+P / outliner)
bool CmdPrefabReparent(std::istringstream& ss, std::string& err) {
    std::string n, path, dest, k; bool fault = false; ss >> n >> path >> dest;
    while (ss >> k) if (k == "fails") fault = true;
    Object* o = NULL;
    if (n == "-") o = SceneCollection ? FindObjectByName(SceneCollection, path) : NULL;
    else { InstancePrefab* ip = Inst(n, err, "prefabreparent"); if (!ip) return false; o = FindNode(ip, path); }
    Object* d = (dest == "-") ? SceneCollection : (SceneCollection ? FindObjectByName(SceneCollection, dest) : NULL);
    if (!o || !d) { err = "prefabreparent: objeto no encontrado"; return false; }
    Object* prev = o->Parent;
    ReparentKeepTransform(o, d);
    const bool change = (o->Parent != prev);
    printf("      [prefabreparent] %s -> %s: %s\n", o->name.c_str(), d->name.c_str(), change ? "se mudo" : "no se mudo");
    if (change == fault) { err = change ? "prefabreparent: se mudo y no debia" : "prefabreparent: no se mudo"; return false; }
    return true;
}

bool CmdPrefabDef(std::istringstream& ss, std::string& err) {
    std::string p; ss >> p;
    const unsigned v = W3dPrefabVersion(p);
    const int idx = W3dRootFind(W3D_ROOT_PREFAB, p);
    const bool loaded = idx >= 0 && W3dRoots()[(size_t)idx].root != NULL;
    printf("      [prefabdef] %s: version %u, raiz cargada %d\n", p.c_str(), v, loaded ? 1 : 0);
    if (!v) { err = "prefabdef: no hay definicion de '" + p + "'"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  Create Prefab con Ctrl+Z, vertex anims compartidas y fisica adentro de las instancias
// ---------------------------------------------------------------------------
bool CmdPrefabPick(std::istringstream& ss, std::string& err) {
    std::string n;
    InteractionMode = ObjectMode; state = editNavigation;
    DeselectAll();
    Object* last = NULL;
    while (ss >> n) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        if (!o) { err = "prefabpick: no hay un objeto '" + n + "'"; return false; }
        o->Select();
        last = o;
    }
    ActiveObj = last;
    printf("      [prefabelegir] %d objeto(s) elegidos\n", (int)ObjSelects.size());
    return true;
}

bool Near(float a, float b) { return fabs(a - b) < 1e-3; }
int CountCurves(Object* o) {
    int n = 0;
    InitSceneAnimations();
    for (size_t e = 0; e < SceneAnimations.size(); e++) {
        if (!SceneAnimations[e]) continue;
        const std::vector<AnimationObject>& v = ((int)e == ActiveSceneAnim) ? AnimationObjects : SceneAnimations[e]->objects;
        for (size_t i = 0; i < v.size(); i++) if (v[i].obj == o) n++;
    }
    return n;
}
bool CmdPrefabObj(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o) { err = "prefabobj: no hay un objeto '" + n + "' en la raiz activa"; return false; }
    Object* p = o->Parent ? o->Parent : SceneCollection;
    int idx = -1;
    for (size_t i = 0; p && i < p->Childrens.size(); i++) if (p->Childrens[i] == o) idx = (int)i;
    bool light = false;
    for (size_t i = 0; i < Lights.size(); i++) if ((Object*)Lights[i] == o) light = true;
    const bool cam = ((Object*)CameraActive == o);
    const int curves = CountCurves(o);
    printf("      [prefabobj] '%s': pos (%.3f, %.3f, %.3f) euler (%.2f, %.2f, %.2f) escala (%.3f, %.3f, %.3f) indice %d padre %s luz %d camara %d curvas %d\n",
           n.c_str(), o->pos.x, o->pos.y, o->pos.z, o->rotEuler.x, o->rotEuler.y, o->rotEuler.z, o->scale.x, o->scale.y, o->scale.z,
           idx, p == SceneCollection ? "-" : p->name.c_str(), light ? 1 : 0, cam ? 1 : 0, curves);
    std::string k;
    while (ss >> k) {
        if (k == "pos" || k == "euler" || k == "scale") {
            float x = 0, y = 0, z = 0; ss >> x >> y >> z;
            const Vector3 v = (k == "pos") ? o->pos : (k == "euler") ? o->rotEuler : o->scale;
            if (!Near(v.x, x) || !Near(v.y, y) || !Near(v.z, z)) { err = "prefabobj: '" + k + "' de '" + n + "' no es la esperada"; return false; }
            continue;
        }
        if (k == "ref") { if (!CheckRef(o, ss, "prefabobj", err)) return false; continue; }
        std::string v; if (!(ss >> v)) { err = "prefabobj: falta el valor de '" + k + "'"; return false; }
        if (k == "index") { if (idx != atoi(v.c_str())) { err = "prefabobj: esta en el lugar " + Ent(idx); return false; } }
        else if (k == "parent") { const std::string pn = (p == SceneCollection) ? std::string("-") : p->name; if (pn != v) { err = "prefabobj: cuelga de '" + pn + "'"; return false; } }
        else if (k == "light") { if ((light ? 1 : 0) != atoi(v.c_str())) { err = "prefabobj: luz=" + Ent(light ? 1 : 0); return false; } }
        else if (k == "camera") { if ((cam ? 1 : 0) != atoi(v.c_str())) { err = "prefabobj: camara=" + Ent(cam ? 1 : 0); return false; } }
        else if (k == "curves") { if (curves != atoi(v.c_str())) { err = "prefabobj: lo nombran " + Ent(curves) + " curvas"; return false; } }
        else if (k == "type") { if (v != W3dTypeName(o->getType())) { err = "prefabobj: es un '" + std::string(W3dTypeName(o->getType())) + "'"; return false; } }
        else { err = "prefabobj: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// el objeto raiz de un prefab nuevo pasa a ser 'o' (en lugar del vacio automatico), en la raiz ACTIVA
void ReplaceObjectRoot(Object* o, const std::string& name) {
    Object* empty = NULL;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) if (SceneCollection->Childrens[i] != o) { empty = SceneCollection->Childrens[i]; break; }
    if (empty) {
        std::vector<Object*>& ch = SceneCollection->Childrens;
        for (size_t i = 0; i < ch.size(); i++) if (ch[i] == empty) { ch.erase(ch.begin() + (long)i); break; }
        DeselectAll(); ActiveObj = NULL;
        W3dFreeSubtree(empty);
    }
    o->SetNameObj(name);   // (el nombre del vacio ya esta libre)
}

bool CmdPrefabFlag(std::istringstream& ss, std::string& err) {
    (void)ss;
    const int idx = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Bandera");
    if (idx < 0) { err = "prefabflag: no se pudo crear el prefab"; return false; }
    Mesh* pl = (Mesh*)NewMesh(MeshType(MeshType::plane), NULL, false);
    if (!pl || pl->vertexSize <= 0) { err = "prefabflag: NewMesh fallo"; return false; }
    W3dMeshCreateResource(pl, "BanderaMalla");
    ReplaceObjectRoot(pl, "Bandera");
    VertexAnimation* va = new VertexAnimation(pl, "ondear", false);
    va->startFrame = 1; va->endFrame = 10;
    std::vector<GLfloat> base(pl->vertex, pl->vertex + pl->vertexSize * 3);
    std::vector<GLfloat> height = base;
    for (int i = 0; i < pl->vertexSize; i++) height[(size_t)i * 3 + 1] += 1.0f;
    VertexAnimSetKey(*va, 1, &base[0], NULL, NULL, KfLinear);
    VertexAnimSetKey(*va, 10, &height[0], NULL, NULL, KfLinear);
    NewActiveVertexAnimation(pl, va);
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "prefabbandera: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    printf("      [prefabbandera] prefab 'Bandera': plano de %d vertices con la vertex anim 'ondear' (%d keyframes)\n",
           pl->vertexSize, (int)va->frames.size());
    return true;
}

// la primera vertex anim de lo generado (o del arbol 'o')
VertexAnimation* FirstAnim(Object* o, Mesh** mesh) {
    Mesh* m = ChildType<Mesh>(o, ObjectType::mesh);
    if (mesh) *mesh = m;
    return (m && !m->animations.empty()) ? m->animations[0] : NULL;
}
bool CmdPrefabFrames(std::istringstream& ss, std::string& err) {
    std::string p, k; ss >> p;
    std::vector<InstancePrefab*> v;
    W3dPrefabInstances(p, v);
    std::set<const void*> games;
    for (size_t i = 0; i < v.size(); i++) {
        VertexAnimation* a = FirstAnim(v[i], NULL);
        if (!a || a->frames.empty()) { err = "prefabframes: '" + v[i]->name + "' no genero una vertex anim con frames"; return false; }
        games.insert((const void*)a->frames[0]);
    }
    // la raiz CARGADA del prefab (si esta): sus frames
    const int idx = W3dRootFind(W3D_ROOT_PREFAB, p);
    Object* praiz = (idx >= 0) ? W3dRoots()[(size_t)idx].root : NULL;
    VertexAnimation* ta = praiz ? FirstAnim(praiz, NULL) : NULL;
    const bool withTemplate = ta && !ta->frames.empty() && games.size() == 1 && games.count((const void*)ta->frames[0]);
    printf("      [prefabframes] %s: %d instancia(s), %d juego(s) de frames%s\n", p.c_str(), (int)v.size(), (int)games.size(),
           !ta ? " (la raiz del prefab no esta cargada)" : withTemplate ? " (los mismos que la plantilla)" : " (distintos de la plantilla)");
    while (ss >> k) {
        int val = -1; if (!(ss >> val)) { err = "prefabframes: falta el valor de '" + k + "'"; return false; }
        if (k == "instances" && (int)v.size() != val) { err = "prefabframes: hay " + Ent((long)v.size()) + " instancias"; return false; }
        else if (k == "games" && (int)games.size() != val) { err = "prefabframes: hay " + Ent((long)games.size()) + " juegos de frames"; return false; }
        else if (k == "template" && (withTemplate ? 1 : 0) != val) { err = "prefabframes: plantilla=" + Ent(withTemplate ? 1 : 0); return false; }
        else if (k != "instances" && k != "games" && k != "template") { err = "prefabframes: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// un keyframe nuevo en la vertex anim de la PLANTILLA (la raiz cargada del prefab), sin abrirla: la edicion
// pasa por el COW de siempre (VertexAnimSetKey -> UninstantiateFrames)
bool CmdPrefabVaKey(std::istringstream& ss, std::string& err) {
    std::string p; int frame = 5; ss >> p >> frame;
    const int idx = W3dRootFind(W3D_ROOT_PREFAB, p);
    Object* praiz = (idx >= 0) ? W3dRoots()[(size_t)idx].root : NULL;
    Mesh* m = NULL;
    VertexAnimation* a = praiz ? FirstAnim(praiz, &m) : NULL;
    if (!a || !m || !m->vertex) { err = "prefabvakey: la raiz de '" + p + "' no esta cargada o no tiene vertex anims"; return false; }
    std::vector<GLfloat> pose(m->vertex, m->vertex + m->vertexSize * 3);
    const int k = VertexAnimSetKey(*a, frame, &pose[0], NULL, NULL, KfLinear);
    printf("      [prefabvakey] %s: keyframe %d en el %d (la plantilla tiene %d keyframes, %s)\n", p.c_str(), k, frame,
           (int)a->frames.size(), a->dataRec ? "shared" : "own");
    if (k < 0) { err = "prefabvakey: no se pudo insertar el keyframe"; return false; }
    return true;
}

bool CmdPrefabVaPose(std::istringstream& ss, std::string& err) {
    std::string a1, a2; ss >> a1 >> a2;
    InstancePrefab* i1 = Inst(a1, err, "prefabvapose"); if (!i1) return false;
    InstancePrefab* i2 = Inst(a2, err, "prefabvapose"); if (!i2) return false;
    Mesh* m1 = NULL; Mesh* m2 = NULL;
    VertexAnimation* va1 = FirstAnim(i1, &m1);
    VertexAnimation* va2 = FirstAnim(i2, &m2);
    if (!va1 || !va2 || va1->frames.empty() || va2->frames.empty()) { err = "prefabvapose: sin vertex anims generadas"; return false; }
    // (B primero: posar A despues no la puede mover)
    EvalVertexAnim(*va2, m2, 1.0f);
    EvalVertexAnim(*va1, m1, 10.0f);
    double d = 0.0, rest = 0.0;
    const GLfloat* f1 = va2->frames[0]->positions;
    for (int i = 0; i < m1->vertexSize * 3 && i < m2->vertexSize * 3; i++) {
        d += fabs(m1->vertex[i] - m2->vertex[i]);
        if (f1) rest += fabs(m2->vertex[i] - f1[i]);
    }
    printf("      [prefabvapose] %s (frame 10) vs %s (frame 1): diferencia %.4f, %s fuera de reposo %.4f\n", a1.c_str(), a2.c_str(), d, a2.c_str(), rest);
    EvalVertexAnim(*va1, m1, 1.0f);
    if (m1->vertex == m2->vertex || d < 1e-3) { err = "prefabvapose: las dos instancias tienen la misma pose"; return false; }
    if (rest > 1e-4) { err = "prefabvapose: posar una instancia movio a la otra"; return false; }
    return true;
}

bool CmdPrefabStack(std::istringstream& ss, std::string& err) {
    (void)ss;
    // el PISO (en la raiz activa): una caja estatica cuya cara de arriba es y = 0
    Mesh* floor = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    if (!floor) { err = "prefabstack: NewMesh fallo"; return false; }
    floor->SetNameObj("Piso");
    W3dMeshCreateResource(floor, "PisoMalla");
    floor->pos = Vector3(0, -1, 0);
    floor->scale = Vector3(30, 1, 30);
    W3dRigidDef* dp = new W3dRigidDef();
    dp->type = 0;
    for (int q = 0; q < 3; q++) { dp->box[q] = 2.0f; dp->center[q] = 0.0f; }
    floor->physics = dp;
    // el PREFAB: Pila (vacio) -> Base (vacio) -> Bloque (cubo con cuerpo dinamico)
    const int idx = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Pila");
    if (idx < 0 || SceneCollection->Childrens.empty()) { err = "prefabstack: no se pudo crear el prefab"; return false; }
    Object* root = SceneCollection->Childrens[0];
    Empty* base = new Empty(root, Vector3(1, 1, 0));
    base->SetNameObj("Base");
    Mesh* b = (Mesh*)NewMesh(MeshType(MeshType::cube), base, false);
    if (!b) { err = "prefabstack: NewMesh (bloque) fallo"; return false; }
    b->SetNameObj("Bloque");
    b->pos = Vector3(0, 0, 0);
    W3dMeshCreateResource(b, "BloqueMalla");
    W3dRigidDef* d = new W3dRigidDef();
    d->type = 1; d->mass = 1.0f;
    for (int q = 0; q < 3; q++) { d->box[q] = 2.0f; d->center[q] = 0.0f; }
    b->physics = d;
    Script(b, "caja.lua");
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "prefabpila: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    printf("      [prefabpila] Piso estatico + prefab 'Pila' (Pila > Base > Bloque con fisica)\n");
    return true;
}

bool CmdPrefabRigid(std::istringstream& ss, std::string& err) {
    std::string n, path, k; ss >> n >> path;
    InstancePrefab* ip = Inst(n, err, "prefabrigid"); if (!ip) return false;
    Object* o = FindNode(ip, path);
    if (!o) { err = "prefabrigid: no hay nada en '" + path + "'"; return false; }
    int which = -1;
    Vector3 center, semi, axes[3];
    for (int i = 0; i < W3dRigidCount() && which < 0; i++) {
        Object* c = NULL;
        if (W3dRigidBox(i, &c, &center, axes, &semi, NULL, NULL) && c == o) which = i;
    }
    if (which >= 0) printf("      [prefabrigido] %s:%s cuerpo #%d centro (%.3f, %.3f, %.3f) semiejes (%.3f, %.3f, %.3f)\n", n.c_str(), path.c_str(),
                          which, center.x, center.y, center.z, semi.x, semi.y, semi.z);
    else printf("      [prefabrigido] %s:%s sin cuerpo\n", n.c_str(), path.c_str());
    while (ss >> k) {
        if (k == "body") { int v = 0; ss >> v; if ((which >= 0 ? 1 : 0) != v) { err = std::string("prefabrigido: ") + (which >= 0 ? "has" : "no tiene") + " cuerpo"; return false; } continue; }
        if (which < 0) { err = "prefabrigid: '" + path + "' no tiene cuerpo"; return false; }
        if (k == "semi") { float v = 0; ss >> v; if (!Near(semi.x, v)) { err = "prefabrigid: el semieje x es " + Ent((long)(semi.x * 1000)) + "/1000"; return false; } continue; }
        if (k == "center") {
            std::string axis, op; float v = 0; ss >> axis >> op >> v;
            const float c = (axis == "x") ? center.x : (axis == "y") ? center.y : center.z;
            if ((op == "less" && !(c < v)) || (op == "greater" && !(c > v))) { err = "prefabrigid: el centro en " + axis + " no es " + op; return false; }
            continue;
        }
        err = "prefabrigid: no entiendo '" + k + "'"; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  el juego compilado
// ---------------------------------------------------------------------------
bool CmdGamePrefabsMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "gameprefabsmin: uso: gameprefabsmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("prefabenemy " + dir, e2)) { err = "juegoprefabsmin: " + e2; return false; }
    InteractionMode = ObjectMode; state = editNavigation;
    for (int i = 0; i < 3; i++) {
        std::string reason;
        InstancePrefab* ip = W3dPrefabAdd("Enemigo", Vector3(-4.0f + 4.0f * (float)i, 0.0f, -6.0f), &reason);
        if (!ip) { err = "juegoprefabsmin: " + reason; return false; }
        if (i == 1) {
            // el del medio con otra vida: el override de la propiedad (como la tarjeta Scripts de lo generado)
            W3dPrefabOverrideProp(ip, "vida", "25");
        }
    }
    Empty* dirObj = new Empty(NULL, Vector3(0, -10, 0));
    dirObj->SetNameObj("Director");
    Script(dirObj, "director.lua");
    DeselectAll(); ActiveObj = NULL;
    printf("      [juegoprefabsmin] prefabenemigo + 3 instancias (Enemigo, Enemigo.001 con vida=25, Enemigo.002) + Director en '%s'\n", dir.c_str());
    return true;
}

bool CmdGamePrefabsLog(std::istringstream& ss, std::string& err) {
    std::string dir, name; ss >> dir >> name;
    if (dir.empty() || name.empty()) { err = "gameprefabslog: uso: gameprefabslog <carpeta> <nombre>"; return false; }
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "gameprefabslog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "gameprefabslog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string verdict;
    int generated = 0;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[prefabs] ");
        if (p && (strstr(p, "OK") || strstr(p, "FALTA")) && strstr(p, "creados=")) verdict = p;
    }
    fclose(f);
    (void)generated;
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [juegoprefabslog] salida=%d | %s\n", r, verdict.empty() ? "(sin veredicto del director)" : verdict.c_str());
    if (verdict.find("[prefabs] OK") == std::string::npos) {
        err = "gameprefabslog: en el juego compilado instanciar()/destruir() no dieron lo mismo que en el Play";
        return false;
    }
    std::string k;
    while (ss >> k) if (verdict.find(k) == std::string::npos) { err = "gameprefabslog: el veredicto no dice '" + k + "'"; return false; }
    return true;
}

} // namespace

// ============================================================================
//  el despachador
// ============================================================================
bool W3dTestsPrefabsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "prefabenemy")       return CmdPrefabEnemy(ss, err);
    if (cmd == "prefabinstantiate")    return CmdPrefabInstantiate(ss, err);
    if (cmd == "prefabinfo")          return CmdPrefabInfo(ss, err);
    if (cmd == "prefabnode")          return CmdPrefabNode(ss, err);
    if (cmd == "prefabscope")         return CmdPrefabScope(ss, err);
    if (cmd == "prefabskin")          return CmdPrefabSkin(ss, err);
    if (cmd == "prefabshared")    return CmdPrefabShared(ss, err);
    if (cmd == "prefabclip")          return CmdPrefabClip(ss, err);
    if (cmd == "prefabpose")          return CmdPrefabPose(ss, err);
    if (cmd == "prefabregenerate")     return CmdPrefabRegenerate(ss, err);
    if (cmd == "prefabcount")        return CmdPrefabCount(ss, err);
    if (cmd == "prefabcreate")         return CmdPrefabCreate(ss, err);
    if (cmd == "prefabunpack") return CmdPrefabUnpack(ss, err);
    if (cmd == "prefabchange")       return CmdPrefabChange(ss, err);
    if (cmd == "prefabreset")         return CmdPrefabReset(ss, err);
    if (cmd == "prefabh")             return CmdPrefabH(ss, err);
    if (cmd == "prefaborigin")        return CmdPrefabOrigin(ss, err);
    if (cmd == "prefabkey")           return CmdPrefabKey(ss, err);
    if (cmd == "prefabadd")           return CmdPrefabAdd(ss, err);
    if (cmd == "prefabmenu")          return CmdPrefabMenu(ss, err);
    if (cmd == "scenemenu")          return CmdSceneMenu(ss, err);
    if (cmd == "scenemenupick")    return CmdSceneMenuPick(ss, err);
    if (cmd == "prefabmenupick")    return CmdPrefabMenuPick(ss, err);
    if (cmd == "prefaboverride")      return CmdPrefabOverride(ss, err);
    if (cmd == "prefablive")         return CmdPrefabLive(ss, err);
    if (cmd == "prefabworld")         return CmdPrefabWorld(ss, err);
    if (cmd == "prefabdef")           return CmdPrefabDef(ss, err);
    if (cmd == "prefabsel")           return CmdPrefabSel(ss, err);
    if (cmd == "prefabreparent")      return CmdPrefabReparent(ss, err);
    if (cmd == "prefabhier")           return CmdPrefabHier(ss, err);
    if (cmd == "gameprefabsmin")     return CmdGamePrefabsMin(ss, err);
    if (cmd == "prefabpick")        return CmdPrefabPick(ss, err);
    if (cmd == "prefabobj")           return CmdPrefabObj(ss, err);
    if (cmd == "prefabflag")       return CmdPrefabFlag(ss, err);
    if (cmd == "prefabframes")        return CmdPrefabFrames(ss, err);
    if (cmd == "prefabvapose")        return CmdPrefabVaPose(ss, err);
    if (cmd == "prefabvakey")         return CmdPrefabVaKey(ss, err);
    if (cmd == "prefabstack")          return CmdPrefabStack(ss, err);
    if (cmd == "prefabrigid")        return CmdPrefabRigid(ss, err);
    if (cmd == "gameprefabslog")     return CmdGamePrefabsLog(ss, err);
    handled = false;
    return false;
}
