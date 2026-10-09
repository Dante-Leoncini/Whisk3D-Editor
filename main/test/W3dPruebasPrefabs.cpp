// ============================================================================
//  W3dPruebasPrefabs.cpp — comandos de harness de las INSTANCIAS DE PREFAB
//  (objects/InstanciaPrefab.h, io/Prefabs.h, io/PrefabsEditor.h).
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
//    prefabinstanciar <prefab> <x> <y> <z> [veces N] [nombre N]   una instancia en ese punto (W3dPrefabAgregar,
//        la puerta de Add > Prefab y de soltarlo de la biblioteca: con Ctrl+Z)
//    prefabinfo <instancia> [prefab P] [generados N] [vacia 0|1] [overrides N] [raiz R]
//    prefabnodo <instancia> <ruta> [nada] [nombre N] [tipo T] [visible 0|1] [generado 0|1] [ref <prop> <valor|->]
//               [curvas N]
//        el objeto generado de esa ruta (relativa al objeto raiz generado: "." es el, como las de los
//        clips de jerarquia)
//    prefabscope <instancia> <ruta> <nombre> <esperado>
//        W3dBuscarNombreDesde desde ese objeto generado: el hallado tiene que ser de la instancia
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
//    simcache 0|1                                    el cache del rewind del Play (para medir el heap sin el)
//    juegoprefabsmin <carpeta>
//        prefabenemigo + 3 instancias en la escena (la del medio con el override vida=25) + un
//        "Director" (director.lua) que en el Play instancia y destruye enemigos por lua (100 ciclos
//        de 10) y deja el veredicto en compartido("juez") y en el log ("[prefabs] OK ...")
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
#include "test/W3dPruebasPrefabs.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "objects/Objects.h"
#include "objects/InstanciaPrefab.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Armature.h"
#include "objects/Empty.h"
#include "objects/Hitbox.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"
#include "io/JsonW3d.h"                // prefaborigen: el lector de siempre
#include "undo/Undo.h"                 // prefabh: los pasos de seleccion y visibilidad
#include "io/RaicesEditor.h"
#include "io/MallasProyecto.h"         // W3dMallaCrearRecurso
#include "objects/ObjectMode.h"        // ReparentKeepTransform (la puerta de reparent)
#include "W3dRaices.h"
#include "animation/SkeletalAnimation.h"
#include "animation/Animation.h"
#include "animation/W3dAnimSet.h"
#include "animation/VertexAnimation.h" // prefabbandera / prefabframes / prefabvapose
#include "objects/Light.h"            // prefabobj: las luces de la raiz
#include "objects/Camera.h"           // prefabobj: la camara activa
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"          // WeightPaintAsegurarMapa
#include "physics/W3dRigido.h"
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
const char* kLuaEnemigo =
    "-- un enemigo del prefab de prueba: cada instancia se maneja a si misma\n"
    "propiedades = { cuerpo = \"objeto\", vida = 10 }\n"
    "function inicio()\n"
    "  local mio = buscar(\"Cuerpo\", yo())      -- el cuerpo de MI esqueleto\n"
    "  local visto = buscar(\"Cuerpo\")          -- sin raiz: por scope (primero MI instancia)\n"
    "  local ref = objeto(\"cuerpo\")            -- la referencia de la tarjeta: por scope tambien\n"
    "  if mio ~= nil and visto == mio and ref == mio then\n"
    "    setCompartido(\"scope_ok\", (compartido(\"scope_ok\") or 0) + 1)\n"
    "  end\n"
    "  setCompartido(\"enemigos\", (compartido(\"enemigos\") or 0) + 1)\n"
    "  setCompartido(\"vida_total\", (compartido(\"vida_total\") or 0) + propiedad(\"vida\"))\n"
    "end\n"
    "function actualizar(dt)\n"
    "  setCompartido(\"latidos\", (compartido(\"latidos\") or 0) + 1)\n"
    "end\n";
const char* kLuaCaja =
    "-- la caja con fisica del prefab: arranca cayendo (los dinamicos nacen dormidos)\n"
    "function inicio() fisicaVel(yo(), 0, -4, 0) end\n";
// EL DIRECTOR: instancia y destruye enemigos EN JUEGO; al final, el veredicto
const char* kLuaDirector =
    "-- director de la prueba de prefabs: instanciar()/destruir() en juego, con un juez al final\n"
    "propiedades = { ciclos = 100, porCiclo = 10, escena = 3 }\n"
    "local vivos = {}\n"
    "local ciclo, creados, fase, espera, lat0 = 0, 0, 0, 0, 0\n"
    "local viejo = nil\n"
    "-- lat1/primer: los latidos entre este frame y el siguiente al primer instanciar(): lo creado corre su actualizar()\n"
    "-- desde el frame SIGUIENTE (y despues del director, que esta antes en el arbol), asi que solo cuentan los de\n"
    "-- la escena. hud: una escena UI no se destruye con destruir() (el juego compilado la seguia usando)\n"
    "local lat1, primer, hud = 0, -1, false\n"
    "function actualizar(dt)\n"
    "  if fase == 0 then\n"
    "    ciclo = ciclo + 1\n"
    "    if ciclo == 1 then\n"
    "      lat1 = compartido(\"latidos\") or 0\n"
    "      hud = (buscar(\"HUD\") ~= nil) and not destruir(buscar(\"HUD\"))\n"
    "    elseif ciclo == 2 then primer = (compartido(\"latidos\") or 0) - lat1 end\n"
    "    for i = 1, #vivos do destruir(vivos[i]) end\n"
    "    if #vivos > 0 then viejo = vivos[1] end\n"
    "    vivos = {}\n"
    "    if ciclo <= propiedad(\"ciclos\") then\n"
    "      for i = 1, propiedad(\"porCiclo\") do\n"
    "        local e = instanciar(\"Enemigo\", i * 2, 0, -20, 90)\n"
    "        if e then vivos[#vivos + 1] = e; creados = creados + 1 end\n"
    "      end\n"
    "    else fase = 1 end\n"
    "    return\n"
    "  end\n"
    "  if fase == 1 then lat0 = compartido(\"latidos\") or 0; fase = 2; espera = 0; return end\n"
    "  if fase == 2 then\n"
    "    espera = espera + 1\n"
    "    if espera < 5 then return end\n"
    "    local lat = (compartido(\"latidos\") or 0) - lat0\n"
    "    local esc = propiedad(\"escena\")\n"
    "    local n = esc + creados\n"
    "    local muerto = (viejo ~= nil) and (tipo(viejo) == \"\")\n"
    "    local ok = creados == propiedad(\"ciclos\") * propiedad(\"porCiclo\") and (compartido(\"enemigos\") or 0) == n\n"
    "      and (compartido(\"scope_ok\") or 0) == n and lat == esc * 5 and muerto and primer == esc and hud\n"
    "    local linea = string.format(\"[prefabs] %s creados=%d enemigos=%d scope=%d latidos=%d muerto=%s vida=%d primer=%d hud=%s\",\n"
    "      ok and \"OK\" or \"FALTA\", creados, compartido(\"enemigos\") or -1, compartido(\"scope_ok\") or -1, lat,\n"
    "      tostring(muerto), compartido(\"vida_total\") or -1, primer, tostring(hud))\n"
    "    info(linea)\n"
    "    setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    fase = 3\n"
    "    salir()\n"
    "  end\n"
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
std::string Ent(long n) { char b[32]; sprintf(b, "%ld", n); return b; }

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

// la instancia por nombre (las instancias son de la escena: su nombre es unico en el scope global)
InstanciaPrefab* Inst(const std::string& n, std::string& err, const char* quien) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    // (un PROXY W3D de una libreria tambien: genera con la misma maquinaria)
    if (!o || !W3dEsTipoInstancia(o->getType())) { err = std::string(quien) + ": no hay una instancia '" + n + "'"; return NULL; }
    return (InstanciaPrefab*)o;
}
// el objeto generado de una ruta (relativa al objeto raiz generado)
Object* Nodo(InstanciaPrefab* ip, const std::string& ruta) {
    Object* r = ip ? ip->RaizGenerada() : NULL;
    return r ? W3dJerNodo(r, ruta) : NULL;
}
int Contar(Object* o) {
    int n = 0;
    for (size_t i = 0; i < o->Childrens.size(); i++) n += 1 + Contar(o->Childrens[i]);
    return n;
}
void JuntarInst(Object* o, std::vector<InstanciaPrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (W3dEsTipoInstancia(o->Childrens[i]->getType())) out.push_back((InstanciaPrefab*)o->Childrens[i]);
        JuntarInst(o->Childrens[i], out);
    }
}
template <class T> T* HijoTipo(Object* o, ObjectType t) {
    if (!o) return NULL;
    if (o->getType() == t) return (T*)o;
    for (size_t i = 0; i < o->Childrens.size(); i++) { T* r = HijoTipo<T>(o->Childrens[i], t); if (r) return r; }
    return NULL;
}
// el valor de la propiedad 'prop' en el PRIMER script de 'o' que la tiene ("" + false = ninguno la tiene)
bool ValorRef(Object* o, const std::string& prop, std::string& out) {
    if (!o || !o->scriptDatos) return false;
    for (size_t s = 0; s < o->scriptDatos->scripts.size(); s++) {
        const std::vector<std::pair<std::string, std::string> >& r = o->scriptDatos->scripts[s].refs;
        for (size_t i = 0; i < r.size(); i++) if (r[i].first == prop) { out = r[i].second; return true; }
    }
    return false;
}
// 'ref <prop> <valor>' de prefabnodo / prefabobj ('-' = ningun script la tiene)
bool ChequearRef(Object* o, std::istringstream& ss, const char* quien, std::string& err) {
    std::string prop, esperado; ss >> prop >> esperado;
    std::string v;
    const bool hay = ValorRef(o, prop, v);
    if (!hay && esperado == "-") return true;
    if (!hay || v != esperado) {
        err = std::string(quien) + ": la ref '" + prop + "' de '" + o->name + "' es '" + (hay ? v : std::string("(ninguna)")) + "', se esperaba '" + esperado + "'";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  prefabenemigo
// ---------------------------------------------------------------------------
bool CmdPrefabEnemigo(std::istringstream& ss, std::string& err) {
    std::string dir, k; int fisica = 0;
    ss >> dir;
    while (ss >> k) { if (k == "fisica") ss >> fisica; }
    if (dir.empty()) { err = "prefabenemigo: uso: prefabenemigo <carpeta> [fisica 0|1]"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "prefabenemigo: " + e2; return false; }
    if (!Escribir(dir + "/enemigo.lua", kLuaEnemigo) || !Escribir(dir + "/caja.lua", kLuaCaja) ||
        !Escribir(dir + "/director.lua", kLuaDirector)) {
        err = "prefabenemigo: no pude escribir los .lua en " + dir; return false;
    }
    // el PREFAB: su raiz (aislada) con el objeto raiz que genera
    const int idx = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Enemigo");
    if (idx < 0 || SceneCollection->Childrens.empty()) { err = "prefabenemigo: no se pudo crear el prefab"; return false; }
    Object* raiz = SceneCollection->Childrens[0];
    Armature* a = new Armature(raiz, Vector3(0, 0, 0));
    a->SetNameObj("Esqueleto");
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0, 0, 0); r.tail = Vector3(0, 1, 0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0, 1, 0); c.tail = Vector3(0, 2, 0); a->bones.push_back(c); }
    PrepararSkinAutorado(a);
    a->animations.push_back(Clip("Saludo", 1, AnimZ, 0.0f, 60.0f, 20));
    a->animations.push_back(Clip("Quieto", 0, AnimY, 0.0f, 35.0f, 30));
    a->animActiva = 0;
    Mesh* s = (Mesh*)NewMesh(MeshType(MeshType::cylinder), a, false);
    if (!s) { err = "prefabenemigo: NewMesh fallo"; return false; }
    s->SetNameObj("Cuerpo");
    s->pos = Vector3(0, 0, 0);
    s->GenerarRender();
    WeightPaintAsegurarMapa(s);
    VertexGroup* vr = new VertexGroup("raiz");
    VertexGroup* vb = new VertexGroup("brazo");
    std::vector<char> hecho(s->vertexSize, 0);
    for (int i = 0; i < s->vertexSize; i++) {
        const int cp = s->vertCtrlPoint[i];
        if (cp < 0 || cp >= (int)hecho.size() || hecho[cp]) continue;
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
    if (!W3dMallaCrearRecurso(s, "Cuerpo")) { err = "prefabenemigo: no pude crear el recurso 'Cuerpo'"; return false; }
    Hitbox* h = HitboxCrearAjustado(a);
    if (!h) { err = "prefabenemigo: HitboxCrearAjustado fallo"; return false; }
    h->SetNameObj("Sensor");
    h->filtro = "jugador";
    Script(a, "enemigo.lua"); Ref(a, "cuerpo", "Cuerpo"); Ref(a, "vida", "10");
    int nPrefabs = 1;
    if (fisica) {
        // un SEGUNDO prefab "Caja": su objeto raiz ES un cubo con cuerpo rigido dinamico
        const int ic = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Caja");
        if (ic < 0) { err = "prefabenemigo: no se pudo crear el prefab Caja"; return false; }
        Object* vacio = SceneCollection->Childrens.empty() ? NULL : SceneCollection->Childrens[0];
        Mesh* cubo = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
        cubo->SetNameObj("Caja");
        W3dMallaCrearRecurso(cubo, "CajaMalla");
        W3dRigidoDef* d = new W3dRigidoDef();
        d->tipo = 1; d->masa = 1.0f;
        for (int q = 0; q < 3; q++) { d->caja[q] = 2.0f; d->centro[q] = 0.0f; }
        cubo->fisica = d;
        Script(cubo, "caja.lua");
        // el cubo reemplaza al vacio automatico como objeto raiz
        if (vacio) {
            std::vector<Object*>& ch = SceneCollection->Childrens;
            for (size_t i = 0; i < ch.size(); i++) if (ch[i] == vacio) { ch.erase(ch.begin() + (long)i); break; }
            DeseleccionarTodo(); ObjActivo = NULL;
            W3dLiberarSubarbol(vacio);
        }
        cubo->SetNameObj("Caja");   // (el nombre del vacio ya esta libre)
        nPrefabs = 2;
    }
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "prefabenemigo: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [prefabenemigo] juego3dmin + %d prefab(s) (Enemigo: Esqueleto + Cuerpo skinneado + Sensor + enemigo.lua%s) en '%s'\n",
           nPrefabs, fisica ? "; Caja: cubo con fisica" : "", dir.c_str());
    return true;
}

bool CmdPrefabInstanciar(std::istringstream& ss, std::string& err) {
    std::string p, k, esperado; float x = 0, y = 0, z = 0; int veces = 1;
    ss >> p >> x >> y >> z;
    while (ss >> k) { if (k == "nombre") ss >> esperado; else if (k == "veces") ss >> veces; }
    std::string motivo;
    InstanciaPrefab* ip = NULL;
    // 'veces N': N instancias en fila (x, x+2, x+4...); 'nombre' se compara con la ultima
    for (int i = 0; i < (veces < 1 ? 1 : veces); i++) {
        ip = W3dPrefabAgregar(p, Vector3(x + 2.0f * (float)i, y, z), &motivo);
        if (!ip) break;
    }
    printf("      [prefabinstanciar] %s x%d en (%g, %g, %g): %s %s\n", p.c_str(), veces, x, y, z, ip ? ip->name.c_str() : "NO", motivo.c_str());
    if (!ip) { err = "prefabinstanciar: " + motivo; return false; }
    if (!esperado.empty() && ip->name != esperado) { err = "prefabinstanciar: se llamo '" + ip->name + "' y se esperaba '" + esperado + "'"; return false; }
    return true;
}

bool CmdPrefabInfo(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    InstanciaPrefab* ip = Inst(n, err, "prefabinfo"); if (!ip) return false;
    const int gen = Contar(ip);
    Object* r = ip->RaizGenerada();
    W3dPrefabSincronizarOverrides(ip);
    const int ov = (int)(ip->overProps.size() + ip->overVisible.size());
    printf("      [prefabinfo] '%s': prefab '%s' generados=%d raiz=%s vacia=%d version=%u overrides=%d (props %d, visible %d)\n",
           ip->name.c_str(), ip->prefab.c_str(), gen, r ? r->name.c_str() : "-", ip->noGenerada ? 1 : 0, ip->versionGenerada,
           ov, (int)ip->overProps.size(), (int)ip->overVisible.size());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "prefabinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "prefab" && ip->prefab != v) { err = "prefabinfo: el prefab es '" + ip->prefab + "'"; return false; }
        if (k == "generados" && gen != atoi(v.c_str())) { err = "prefabinfo: genero " + Ent(gen) + " objetos, se esperaban " + v; return false; }
        if (k == "vacia" && (ip->noGenerada ? 1 : 0) != atoi(v.c_str())) { err = "prefabinfo: vacia=" + Ent(ip->noGenerada ? 1 : 0); return false; }
        if (k == "overrides" && ov != atoi(v.c_str())) { err = "prefabinfo: tiene " + Ent(ov) + " overrides, se esperaban " + v; return false; }
        if (k == "raiz" && (!r || r->name != v)) { err = "prefabinfo: el objeto raiz generado no es '" + v + "'"; return false; }
        if (k != "prefab" && k != "generados" && k != "vacia" && k != "overrides" && k != "raiz") { err = "prefabinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

int ContarCurvas(Object* o);   // (mas abajo: las curvas de las animaciones de escena que lo nombran)
bool CmdPrefabNodo(std::istringstream& ss, std::string& err) {
    std::string n, ruta; ss >> n >> ruta;
    InstanciaPrefab* ip = Inst(n, err, "prefabnodo"); if (!ip) return false;
    Object* o = Nodo(ip, ruta);
    printf("      [prefabnodo] %s:%s -> %s (%s) visible=%d generado=%d\n", n.c_str(), ruta.c_str(), o ? o->name.c_str() : "nada",
           o ? W3dNombreTipo(o->getType()) : "-", o ? (int)o->visible : -1, o ? (int)W3dEsGenerado(o) : -1);
    std::string k;
    while (ss >> k) {
        if (k == "nada") { if (o) { err = "prefabnodo: la ruta existe"; return false; } continue; }
        if (!o) { err = "prefabnodo: no hay nada en '" + ruta + "'"; return false; }
        if (k == "ref") { if (!ChequearRef(o, ss, "prefabnodo", err)) return false; continue; }
        std::string v; if (!(ss >> v)) { err = "prefabnodo: falta el valor de '" + k + "'"; return false; }
        if (k == "nombre" && o->name != v) { err = "prefabnodo: se llama '" + o->name + "'"; return false; }
        if (k == "tipo" && std::string(W3dNombreTipo(o->getType())) != v) { err = "prefabnodo: es un '" + std::string(W3dNombreTipo(o->getType())) + "'"; return false; }
        if (k == "visible" && (int)o->visible != atoi(v.c_str())) { err = "prefabnodo: visible=" + Ent(o->visible); return false; }
        if (k == "generado" && (int)W3dEsGenerado(o) != atoi(v.c_str())) { err = "prefabnodo: generado=" + Ent(W3dEsGenerado(o)); return false; }
        if (k == "curvas" && ContarCurvas(o) != atoi(v.c_str())) { err = "prefabnodo: lo nombran " + Ent(ContarCurvas(o)) + " curvas"; return false; }
    }
    return true;
}

bool CmdPrefabScope(std::istringstream& ss, std::string& err) {
    std::string n, ruta, nombre, esperado; ss >> n >> ruta >> nombre >> esperado;
    InstanciaPrefab* ip = Inst(n, err, "prefabscope"); if (!ip) return false;
    Object* desde = Nodo(ip, ruta);
    if (!desde) { err = "prefabscope: no hay nada en '" + ruta + "'"; return false; }
    Object* o = W3dBuscarNombreDesde(desde, nombre);
    Object* de = o ? W3dInstanciaDe(o) : NULL;
    const std::string sDe = !o ? std::string("nada") : (de ? de->name : std::string("-"));
    printf("      [prefabscope] desde %s:%s busca '%s' -> %s (de %s)\n", n.c_str(), ruta.c_str(), nombre.c_str(), o ? o->name.c_str() : "nada", sDe.c_str());
    if (sDe != esperado) { err = "prefabscope: se encontro el de '" + sDe + "' y se esperaba '" + esperado + "'"; return false; }
    return true;
}

bool CmdPrefabSkin(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    InstanciaPrefab* ip = Inst(n, err, "prefabskin"); if (!ip) return false;
    Armature* a = HijoTipo<Armature>(ip, ObjectType::armature);
    Mesh* m = HijoTipo<Mesh>(ip, ObjectType::mesh);
    Hitbox* h = HijoTipo<Hitbox>(ip, ObjectType::hitbox);
    if (!a || !m || !h) { err = "prefabskin: la instancia no genero esqueleto, cuerpo y sensor"; return false; }
    Object* tgt = NULL;
    for (size_t i = 0; i < m->modificadores.size(); i++)
        if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature) tgt = m->modificadores[i]->target;
    printf("      [prefabskin] '%s': cuerpo skin=%p mod=%p esqueleto=%p sensor duenio=%p\n", n.c_str(), (void*)m->skinArmature,
           (void*)tgt, (void*)a, (void*)W3dHitboxDuenio(h));
    if ((Object*)m->skinArmature != (Object*)a || tgt != (Object*)a) { err = "prefabskin: el cuerpo no se deforma con el esqueleto de SU instancia"; return false; }
    if (W3dHitboxDuenio(h) != (Object*)a) { err = "prefabskin: el sensor no avisa al esqueleto de SU instancia"; return false; }
    return true;
}

bool CmdPrefabCompartido(std::istringstream& ss, std::string& err) {
    std::string p, k; ss >> p;
    std::vector<InstanciaPrefab*> v;
    W3dPrefabInstancias(p, v);
    std::set<const void*> recs, sets, verts;
    for (size_t i = 0; i < v.size(); i++) {
        Mesh* m = HijoTipo<Mesh>(v[i], ObjectType::mesh);
        Armature* a = HijoTipo<Armature>(v[i], ObjectType::armature);
        if (m) { recs.insert(m->malla); verts.insert(m->vertex); if (m->malla && m->vertex != m->malla->vertex) { err = "prefabcompartido: la geometria de un cuerpo no es la de su recurso"; return false; } }
        if (a) {
            sets.insert(W3dArmatureAnimSetRecurso(a));
            if (!W3dArmatureAnimsCompartidas(a)) { err = "prefabcompartido: el esqueleto de '" + v[i]->name + "' tiene clips propios"; return false; }
        }
    }
    printf("      [prefabcompartido] %s: %d instancia(s), %d recurso(s) de malla, %d geometria(s), %d animset(s)\n", p.c_str(),
           (int)v.size(), (int)recs.size(), (int)verts.size(), (int)sets.size());
    while (ss >> k) {
        int val = -1; if (!(ss >> val)) { err = "prefabcompartido: falta el valor de '" + k + "'"; return false; }
        if (k == "instancias" && (int)v.size() != val) { err = "prefabcompartido: hay " + Ent((long)v.size()) + " instancias"; return false; }
        if (k == "mallas" && ((int)recs.size() != val || (int)verts.size() != val)) { err = "prefabcompartido: " + Ent((long)recs.size()) + " recursos de malla / " + Ent((long)verts.size()) + " geometrias"; return false; }
        if (k == "animsets" && (int)sets.size() != val) { err = "prefabcompartido: " + Ent((long)sets.size()) + " animsets"; return false; }
    }
    return true;
}

bool CmdPrefabClip(std::istringstream& ss, std::string& err) {
    std::string n, ruta, clip; ss >> n >> ruta >> clip;
    InstanciaPrefab* ip = Inst(n, err, "prefabclip"); if (!ip) return false;
    Object* o = Nodo(ip, ruta);
    if (!o || o->getType() != ObjectType::armature) { err = "prefabclip: '" + ruta + "' no es un esqueleto"; return false; }
    Armature* a = (Armature*)o;
    for (size_t i = 0; i < a->animations.size(); i++)
        if (a->animations[i] && a->animations[i]->name == clip) { a->animActiva = (int)i; return true; }
    err = "prefabclip: el esqueleto no tiene el clip '" + clip + "'";
    return false;
}

bool CmdPrefabPose(std::istringstream& ss, std::string& err) {
    std::string a1, a2, modo; int frame = 10; ss >> a1 >> a2 >> frame >> modo;
    InstanciaPrefab* i1 = Inst(a1, err, "prefabpose"); if (!i1) return false;
    InstanciaPrefab* i2 = Inst(a2, err, "prefabpose"); if (!i2) return false;
    Armature* ar[2] = { HijoTipo<Armature>(i1, ObjectType::armature), HijoTipo<Armature>(i2, ObjectType::armature) };
    Mesh* me[2] = { HijoTipo<Mesh>(i1, ObjectType::mesh), HijoTipo<Mesh>(i2, ObjectType::mesh) };
    if (!ar[0] || !ar[1] || !me[0] || !me[1]) { err = "prefabpose: faltan esqueletos/cuerpos"; return false; }
    const int kind = ActiveAnimKind;
    ActiveAnimKind = 2;   // como en el juego: cada esqueleto con SU clip y SU cabezal
    for (int k = 0; k < 2; k++) {
        Armature* a = ar[k];
        if (a->animActiva < 0 || a->animActiva >= (int)a->animations.size()) { ActiveAnimKind = kind; err = "prefabpose: esqueleto sin clip activo"; return false; }
        a->juegoFrame = (float)(frame - a->animations[a->animActiva]->startFrame);
        EvaluarPoseEsqueleto(a, frame);
        me[k]->lastSkinFrame = -999999;
        SkinearMesh(me[k]);
    }
    ActiveAnimKind = kind;
    if (!me[0]->skinVertex || !me[1]->skinVertex) { err = "prefabpose: el skinning no produjo pose"; return false; }
    if (me[0]->skinVertex == me[1]->skinVertex) { err = "prefabpose: los dos cuerpos comparten el buffer de pose"; return false; }
    double d = 0.0;
    for (int i = 0; i < me[0]->vertexSize * 3 && i < me[1]->vertexSize * 3; i++) d += fabs(me[0]->skinVertex[i] - me[1]->skinVertex[i]);
    printf("      [prefabpose] %s vs %s en el frame %d: diferencia %.4f (%s)\n", a1.c_str(), a2.c_str(), frame, d, modo.c_str());
    if (modo == "distinta" && d < 1e-3) { err = "prefabpose: las poses son iguales (se esperaban distintas)"; return false; }
    if (modo == "igual" && d > 1e-5) { err = "prefabpose: las poses difieren (se esperaban iguales)"; return false; }
    return true;
}

bool CmdPrefabRegenerar(std::istringstream& ss, std::string& err) {
    std::string p, k; int esperado = -1; ss >> p;
    while (ss >> k) if (k == "n") ss >> esperado;
    W3dPrefabInvalidar(p);
    const int n = W3dPrefabRegenerarInstancias(p);
    printf("      [prefabregenerar] %s: %d instancia(s)\n", p.c_str(), n);
    if (esperado >= 0 && n != esperado) { err = "prefabregenerar: regenero " + Ent(n) + ", se esperaban " + Ent(esperado); return false; }
    return true;
}

bool CmdPrefabContar(std::istringstream& ss, std::string& err) {
    std::string p; int esperado = -1; ss >> p >> esperado;
    const int n = W3dPrefabContarInstancias(p);
    printf("      [prefabcontar] %s: %d instancia(s)\n", p.c_str(), n);
    if (n != esperado) { err = "prefabcontar: hay " + Ent(n) + ", se esperaban " + Ent(esperado); return false; }
    return true;
}

bool CmdPrefabCrear(std::istringstream& ss, std::string& err) {
    std::string k, esperado; bool falla = false;
    while (ss >> k) { if (k == "nombre") ss >> esperado; else if (k == "falla") falla = true; }
    std::string nom, motivo;
    InteractionMode = ObjectMode; estado = editNavegacion;
    const bool ok = W3dPrefabCrearDesdeSeleccion(&nom, &motivo);
    printf("      [prefabcrear] %s %s\n", ok ? nom.c_str() : "NO", motivo.c_str());
    if (ok == falla) { err = ok ? "prefabcrear: se creo y no debia" : "prefabcrear: " + motivo; return false; }
    if (ok && !esperado.empty() && nom != esperado) { err = "prefabcrear: el prefab se llamo '" + nom + "'"; return false; }
    return true;
}

bool CmdPrefabDesempaquetar(std::istringstream& ss, std::string& err) {
    std::string n, k; bool falla = false; ss >> n;
    while (ss >> k) if (k == "falla") falla = true;
    InstanciaPrefab* ip = Inst(n, err, "prefabdesempaquetar"); if (!ip) return false;
    std::string motivo;
    InteractionMode = ObjectMode; estado = editNavegacion;
    const bool ok = W3dPrefabDesempaquetar(ip, &motivo);
    printf("      [prefabdesempaquetar] %s: %s %s\n", n.c_str(), ok ? "ok" : "NO", motivo.c_str());
    if (ok == falla) { err = ok ? "prefabdesempaquetar: se desempaqueto y no debia" : "prefabdesempaquetar: " + motivo; return false; }
    return true;
}

bool CmdPrefabCambiar(std::istringstream& ss, std::string& err) {
    std::string n, p, k; bool falla = false; ss >> n >> p;
    while (ss >> k) if (k == "falla") falla = true;
    InstanciaPrefab* ip = Inst(n, err, "prefabcambiar"); if (!ip) return false;
    std::string motivo;
    const bool ok = W3dPrefabCambiar(ip, p, &motivo);
    printf("      [prefabcambiar] %s -> %s: %s %s\n", n.c_str(), p.c_str(), ok ? "ok" : "NO", motivo.c_str());
    if (ok == falla) { err = ok ? "prefabcambiar: cambio y no debia" : "prefabcambiar: " + motivo; return false; }
    return true;
}

// prefabreset <instancia> [falla]: "Reset Overrides" de la tarjeta (con Ctrl+Z)
bool CmdPrefabReset(std::istringstream& ss, std::string& err) {
    std::string n, k; bool falla = false; ss >> n;
    while (ss >> k) if (k == "falla") falla = true;
    InstanciaPrefab* ip = Inst(n, err, "prefabreset"); if (!ip) return false;
    std::string motivo;
    const bool ok = W3dPrefabResetOverrides(ip, &motivo);
    printf("      [prefabreset] %s: %s %s\n", n.c_str(), ok ? "ok" : "NO", motivo.c_str());
    if (ok == falla) { err = ok ? "prefabreset: reseteo y no debia" : "prefabreset: " + motivo; return false; }
    return true;
}

// prefabh <instancia> <ruta>: el usuario ELIGE ese objeto generado (un click: su paso de undo de seleccion) y lo
// oculta con H (su paso de undo de visibilidad). Deja en el historial DOS pasos que lo nombran por puntero
bool CmdPrefabH(std::istringstream& ss, std::string& err) {
    std::string n, ruta; ss >> n >> ruta;
    InstanciaPrefab* ip = Inst(n, err, "prefabh"); if (!ip) return false;
    Object* o = Nodo(ip, ruta);
    if (!o) { err = "prefabh: no hay nada en '" + ruta + "'"; return false; }
    InteractionMode = ObjectMode; estado = editNavegacion;
    UndoCapturarSeleccion();
    DeseleccionarTodo(); o->Seleccionar(); ObjActivo = o;
    UndoCapturarVisibilidad();
    ChangeVisibilityObj();
    printf("      [prefabh] %s:%s visible=%d\n", n.c_str(), ruta.c_str(), (int)o->visible);
    return true;
}

// prefabadd cube|empty: el item del menu Add del viewport 3D (la accion de verdad: editando un prefab, lo nuevo va
// adentro de su objeto raiz)
bool CmdPrefabAdd(std::istringstream& ss, std::string& err) {
    std::string t; ss >> t;
    InteractionMode = ObjectMode; estado = editNavegacion;
    if (t == "cube") AddCube();
    else if (t == "empty") AddEmpty();
    else if (t == "proxy") AddProxyW3d();   // (Add > Proxy W3D)
    else { err = "prefabadd: uso: prefabadd cube|empty|proxy"; return false; }
    printf("      [prefabadd] %s -> '%s' (padre %s)\n", t.c_str(), ObjActivo ? ObjActivo->name.c_str() : "-",
           ObjActivo && ObjActivo->Parent && ObjActivo->Parent != SceneCollection ? ObjActivo->Parent->name.c_str() : "-");
    return true;
}

// prefabkey: Insert Keyframe (la I) sobre la seleccion, como el menu
bool CmdPrefabKey(std::istringstream& ss, std::string& err) {
    (void)ss; (void)err;
    InteractionMode = ObjectMode; estado = editNavegacion;
    InsertarKeyframeObjeto(0);
    printf("      [prefabkey] %d elegido(s)\n", (int)ObjSelects.size());
    return true;
}

// prefaborigen <n>: el "_origen" (el serial de la plantilla) de un nodo en seco se lee EXACTO (entero sin signo,
// no por float: desde 2^24 se corria al vecino)
bool CmdPrefabOrigen(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    const std::string json = "{\"_origen\": " + n + "}";
    JParser p(json.data(), json.size());
    JVal* v = p.Valor();
    const unsigned leido = (v && !p.error && v->tipo == 4) ? JU(v, "_origen", 0u) : 0u;
    delete v;
    const unsigned esperado = (unsigned)strtoul(n.c_str(), NULL, 10);
    printf("      [prefaborigen] %s -> %u\n", n.c_str(), leido);
    if (leido != esperado) { err = "prefaborigen: se leyo " + Ent((long)leido) + ", se esperaba " + n; return false; }
    return true;
}

bool CmdPrefabMenu(std::istringstream& ss, std::string& err) {
    PopupMenu m;
    W3dPrefabMenuAddArmar(&m);
    std::vector<std::string> nombres;
    for (size_t i = 0; i < m.items.size(); i++) if (m.items[i]->id > 0) nombres.push_back(m.items[i]->text);
    printf("      [prefabmenu] %d prefab(s):", (int)nombres.size());
    for (size_t i = 0; i < nombres.size(); i++) printf(" %s", nombres[i].c_str());
    printf("\n");
    std::string k, v;
    while (ss >> k >> v) {
        bool esta = false;
        for (size_t i = 0; i < nombres.size(); i++) if (nombres[i] == v) esta = true;
        if (k == "n" && (int)nombres.size() != atoi(v.c_str())) { err = "prefabmenu: tiene " + Ent((long)nombres.size()) + " filas"; return false; }
        if (k == "tiene" && !esta) { err = "prefabmenu: no ofrece '" + v + "'"; return false; }
        if (k == "notiene" && esta) { err = "prefabmenu: ofrece '" + v + "'"; return false; }
    }
    return true;
}

// escenamenu [n N] [tiene E]... [notiene E]...   el submenu Add > Scene (las otras escenas, sin ciclos)
bool CmdEscenaMenu(std::istringstream& ss, std::string& err) {
    PopupMenu m;
    W3dEscenaMenuAddArmar(&m);
    std::vector<std::string> nombres;
    for (size_t i = 0; i < m.items.size(); i++) if (m.items[i]->id > 0) nombres.push_back(m.items[i]->text);
    printf("      [escenamenu] %d escena(s):", (int)nombres.size());
    for (size_t i = 0; i < nombres.size(); i++) printf(" %s", nombres[i].c_str());
    printf("\n");
    std::string k, v;
    while (ss >> k >> v) {
        bool esta = false;
        for (size_t i = 0; i < nombres.size(); i++) if (nombres[i] == v) esta = true;
        if (k == "n" && (int)nombres.size() != atoi(v.c_str())) { err = "escenamenu: tiene " + Ent((long)nombres.size()) + " filas"; return false; }
        if (k == "tiene" && !esta) { err = "escenamenu: no ofrece '" + v + "'"; return false; }
        if (k == "notiene" && esta) { err = "escenamenu: ofrece '" + v + "'"; return false; }
    }
    return true;
}
// escenamenuelegir <escena>   elige esa fila de Add > Scene (una instancia de la escena en el cursor 3D)
bool CmdEscenaMenuElegir(std::istringstream& ss, std::string& err) {
    std::string p; ss >> p;
    if (!MenuEscenasAdd) { err = "escenamenuelegir: no hay menu Add (sin viewport 3D)"; return false; }
    W3dEscenaMenuAddArmar(MenuEscenasAdd);
    for (size_t i = 0; i < MenuEscenasAdd->items.size(); i++)
        if (MenuEscenasAdd->items[i]->text == p && MenuEscenasAdd->items[i]->id > 0) {
            InteractionMode = ObjectMode; estado = editNavegacion;
            MenuEscenasAdd->Ejecutar(MenuEscenasAdd->items[i]->id);
            printf("      [escenamenuelegir] %s -> activo '%s'\n", p.c_str(), ObjActivo ? ObjActivo->name.c_str() : "-");
            if (!ObjActivo || !W3dEsTipoInstancia(ObjActivo->getType())) { err = "escenamenuelegir: no se creo la instancia"; return false; }
            return true;
        }
    err = "escenamenuelegir: el menu no ofrece '" + p + "'";
    return false;
}

bool CmdPrefabMenuElegir(std::istringstream& ss, std::string& err) {
    std::string p; ss >> p;
    if (!MenuPrefabsAdd) { err = "prefabmenuelegir: no hay menu Add (sin viewport 3D)"; return false; }
    W3dPrefabMenuAddArmar(MenuPrefabsAdd);
    for (size_t i = 0; i < MenuPrefabsAdd->items.size(); i++)
        if (MenuPrefabsAdd->items[i]->text == p) {
            InteractionMode = ObjectMode; estado = editNavegacion;
            MenuPrefabsAdd->Ejecutar(MenuPrefabsAdd->items[i]->id);
            printf("      [prefabmenuelegir] %s -> activo '%s'\n", p.c_str(), ObjActivo ? ObjActivo->name.c_str() : "-");
            // (un prefab de una LIBRERIA vinculada crea un PROXY)
            if (!ObjActivo || !W3dEsTipoInstancia(ObjActivo->getType())) { err = "prefabmenuelegir: no se creo la instancia"; return false; }
            return true;
        }
    err = "prefabmenuelegir: el submenu no ofrece '" + p + "'";
    return false;
}

bool CmdPrefabOverride(std::istringstream& ss, std::string& err) {
    std::string n, que; ss >> n >> que;
    InstanciaPrefab* ip = Inst(n, err, "prefaboverride"); if (!ip) return false;
    if (que == "visible") {
        std::string ruta; int v = 1; ss >> ruta >> v;
        Object* o = Nodo(ip, ruta);
        if (!o) { err = "prefaboverride: no hay nada en '" + ruta + "'"; return false; }
        o->visible = (v != 0);
    } else if (que == "prop") {
        std::string ruta, prop, valor; ss >> ruta >> prop >> valor;
        Object* o = Nodo(ip, ruta);
        if (!o || !o->scriptDatos || o->scriptDatos->scripts.empty()) { err = "prefaboverride: '" + ruta + "' no tiene scripts"; return false; }
        // como la tarjeta Scripts: escribe el valor en SU script (la instancia lo nota al sincronizar)
        std::vector<std::pair<std::string, std::string> >& refs = o->scriptDatos->scripts[0].refs;
        bool puesto = false;
        for (size_t r = 0; r < refs.size(); r++) if (refs[r].first == prop) { refs[r].second = valor; puesto = true; }
        if (!puesto) refs.push_back(std::make_pair(prop, valor));
    } else { err = "prefaboverride: visible <ruta> 0|1 | prop <ruta> <prop> <valor>"; return false; }
    W3dPrefabSincronizarOverrides(ip);
    printf("      [prefaboverride] %s: %d prop(s), %d visible(s)\n", n.c_str(), (int)ip->overProps.size(), (int)ip->overVisible.size());
    return true;
}

bool CmdPrefabVivos(std::istringstream& ss, std::string& err) {
    int esperado = -1; ss >> esperado;
    std::vector<InstanciaPrefab*> v;
    JuntarInst(SceneCollection, v);
    printf("      [prefabvivos] %d instancia(s) en la raiz activa\n", (int)v.size());
    if (esperado >= 0 && (int)v.size() != esperado) { err = "prefabvivos: hay " + Ent((long)v.size()) + ", se esperaban " + Ent(esperado); return false; }
    return true;
}

bool CmdPrefabMundo(std::istringstream& ss, std::string& err) {
    std::string n, ruta, eje, op; float valor = 0; ss >> n >> ruta >> eje >> op >> valor;
    InstanciaPrefab* ip = Inst(n, err, "prefabmundo"); if (!ip) return false;
    Object* o = Nodo(ip, ruta);
    if (!o) { err = "prefabmundo: no hay nada en '" + ruta + "'"; return false; }
    const Vector3 w = o->GetGlobalPosition();
    const float c = (eje == "x") ? w.x : (eje == "y") ? w.y : w.z;
    printf("      [prefabmundo] %s:%s en el mundo (%.3f, %.3f, %.3f)\n", n.c_str(), ruta.c_str(), w.x, w.y, w.z);
    if (op == "menor" && !(c < valor)) { err = "prefabmundo: " + eje + " no es menor"; return false; }
    if (op == "mayor" && !(c > valor)) { err = "prefabmundo: " + eje + " no es mayor"; return false; }
    return true;
}

// prefabjer <instancia> <ruta> <clip> [loop 0|1]: reproduce un CLIP DE JERARQUIA en ese objeto generado (el
// clip vino con el prefab: sus rutas usan los nombres del prefab, que la instancia genera tal cual)
bool CmdPrefabJer(std::istringstream& ss, std::string& err) {
    std::string n, ruta, clip, k; int loop = 1; ss >> n >> ruta >> clip;
    while (ss >> k) { if (k == "loop") ss >> loop; else { err = "prefabjer: no entiendo '" + k + "'"; return false; } }
    InstanciaPrefab* ip = Inst(n, err, "prefabjer"); if (!ip) return false;
    Object* o = Nodo(ip, ruta);
    if (!o) { err = "prefabjer: no hay nada en '" + ruta + "'"; return false; }
    W3dClipsVistasSincronizar();
    const float d = W3dAnimObjetoPlay(o, clip, loop != 0);
    printf("      [prefabjer] %s:%s '%s' loop=%d -> %.4f s\n", n.c_str(), ruta.c_str(), clip.c_str(), loop, d);
    if (d < 0.0f) { err = "prefabjer: '" + ruta + "' de '" + n + "' no tiene el clip '" + clip + "'"; return false; }
    return true;
}

// prefabsel <instancia> <ruta>: elige SOLO ese objeto generado (como un click en el outliner)
bool CmdPrefabSel(std::istringstream& ss, std::string& err) {
    std::string n, ruta; ss >> n >> ruta;
    InstanciaPrefab* ip = Inst(n, err, "prefabsel"); if (!ip) return false;
    Object* o = ruta.empty() ? (Object*)ip : Nodo(ip, ruta);
    if (!o) { err = "prefabsel: no hay nada en '" + ruta + "'"; return false; }
    InteractionMode = ObjectMode; estado = editNavegacion;
    DeseleccionarTodo(); o->Seleccionar(); ObjActivo = o;
    return true;
}
// prefabreparent <instancia> <ruta> <nuevoPadre> [falla]: la puerta de reparent de siempre (Ctrl+P / outliner)
bool CmdPrefabReparent(std::istringstream& ss, std::string& err) {
    std::string n, ruta, dest, k; bool falla = false; ss >> n >> ruta >> dest;
    while (ss >> k) if (k == "falla") falla = true;
    Object* o = NULL;
    if (n == "-") o = SceneCollection ? FindObjectByName(SceneCollection, ruta) : NULL;
    else { InstanciaPrefab* ip = Inst(n, err, "prefabreparent"); if (!ip) return false; o = Nodo(ip, ruta); }
    Object* d = (dest == "-") ? SceneCollection : (SceneCollection ? FindObjectByName(SceneCollection, dest) : NULL);
    if (!o || !d) { err = "prefabreparent: objeto no encontrado"; return false; }
    Object* antes = o->Parent;
    ReparentKeepTransform(o, d);
    const bool cambio = (o->Parent != antes);
    printf("      [prefabreparent] %s -> %s: %s\n", o->name.c_str(), d->name.c_str(), cambio ? "se mudo" : "no se mudo");
    if (cambio == falla) { err = cambio ? "prefabreparent: se mudo y no debia" : "prefabreparent: no se mudo"; return false; }
    return true;
}

bool CmdPrefabDef(std::istringstream& ss, std::string& err) {
    std::string p; ss >> p;
    const unsigned v = W3dPrefabVersion(p);
    const int idx = W3dRaizBuscar(W3D_RAIZ_PREFAB, p);
    const bool cargado = idx >= 0 && W3dRaices()[(size_t)idx].raiz != NULL;
    printf("      [prefabdef] %s: version %u, raiz cargada %d\n", p.c_str(), v, cargado ? 1 : 0);
    if (!v) { err = "prefabdef: no hay definicion de '" + p + "'"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  Create Prefab con Ctrl+Z, vertex anims compartidas y fisica adentro de las instancias
// ---------------------------------------------------------------------------
bool CmdPrefabElegir(std::istringstream& ss, std::string& err) {
    std::string n;
    InteractionMode = ObjectMode; estado = editNavegacion;
    DeseleccionarTodo();
    Object* ultimo = NULL;
    while (ss >> n) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        if (!o) { err = "prefabelegir: no hay un objeto '" + n + "'"; return false; }
        o->Seleccionar();
        ultimo = o;
    }
    ObjActivo = ultimo;
    printf("      [prefabelegir] %d objeto(s) elegidos\n", (int)ObjSelects.size());
    return true;
}

bool Cerca(float a, float b) { return fabs(a - b) < 1e-3; }
int ContarCurvas(Object* o) {
    int n = 0;
    InitSceneAnimations();
    for (size_t e = 0; e < SceneAnimations.size(); e++) {
        if (!SceneAnimations[e]) continue;
        const std::vector<AnimationObject>& v = ((int)e == SceneAnimActiva) ? AnimationObjects : SceneAnimations[e]->objetos;
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
    bool luz = false;
    for (size_t i = 0; i < Lights.size(); i++) if ((Object*)Lights[i] == o) luz = true;
    const bool cam = ((Object*)CameraActive == o);
    const int curvas = ContarCurvas(o);
    printf("      [prefabobj] '%s': pos (%.3f, %.3f, %.3f) euler (%.2f, %.2f, %.2f) escala (%.3f, %.3f, %.3f) indice %d padre %s luz %d camara %d curvas %d\n",
           n.c_str(), o->pos.x, o->pos.y, o->pos.z, o->rotEuler.x, o->rotEuler.y, o->rotEuler.z, o->scale.x, o->scale.y, o->scale.z,
           idx, p == SceneCollection ? "-" : p->name.c_str(), luz ? 1 : 0, cam ? 1 : 0, curvas);
    std::string k;
    while (ss >> k) {
        if (k == "pos" || k == "euler" || k == "escala") {
            float x = 0, y = 0, z = 0; ss >> x >> y >> z;
            const Vector3 v = (k == "pos") ? o->pos : (k == "euler") ? o->rotEuler : o->scale;
            if (!Cerca(v.x, x) || !Cerca(v.y, y) || !Cerca(v.z, z)) { err = "prefabobj: '" + k + "' de '" + n + "' no es la esperada"; return false; }
            continue;
        }
        if (k == "ref") { if (!ChequearRef(o, ss, "prefabobj", err)) return false; continue; }
        std::string v; if (!(ss >> v)) { err = "prefabobj: falta el valor de '" + k + "'"; return false; }
        if (k == "indice") { if (idx != atoi(v.c_str())) { err = "prefabobj: esta en el lugar " + Ent(idx); return false; } }
        else if (k == "padre") { const std::string pn = (p == SceneCollection) ? std::string("-") : p->name; if (pn != v) { err = "prefabobj: cuelga de '" + pn + "'"; return false; } }
        else if (k == "luz") { if ((luz ? 1 : 0) != atoi(v.c_str())) { err = "prefabobj: luz=" + Ent(luz ? 1 : 0); return false; } }
        else if (k == "camara") { if ((cam ? 1 : 0) != atoi(v.c_str())) { err = "prefabobj: camara=" + Ent(cam ? 1 : 0); return false; } }
        else if (k == "curvas") { if (curvas != atoi(v.c_str())) { err = "prefabobj: lo nombran " + Ent(curvas) + " curvas"; return false; } }
        else if (k == "tipo") { if (v != W3dNombreTipo(o->getType())) { err = "prefabobj: es un '" + std::string(W3dNombreTipo(o->getType())) + "'"; return false; } }
        else { err = "prefabobj: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// el objeto raiz de un prefab nuevo pasa a ser 'o' (en lugar del vacio automatico), en la raiz ACTIVA
void ReemplazarObjetoRaiz(Object* o, const std::string& nombre) {
    Object* vacio = NULL;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) if (SceneCollection->Childrens[i] != o) { vacio = SceneCollection->Childrens[i]; break; }
    if (vacio) {
        std::vector<Object*>& ch = SceneCollection->Childrens;
        for (size_t i = 0; i < ch.size(); i++) if (ch[i] == vacio) { ch.erase(ch.begin() + (long)i); break; }
        DeseleccionarTodo(); ObjActivo = NULL;
        W3dLiberarSubarbol(vacio);
    }
    o->SetNameObj(nombre);   // (el nombre del vacio ya esta libre)
}

bool CmdPrefabBandera(std::istringstream& ss, std::string& err) {
    (void)ss;
    const int idx = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Bandera");
    if (idx < 0) { err = "prefabbandera: no se pudo crear el prefab"; return false; }
    Mesh* pl = (Mesh*)NewMesh(MeshType(MeshType::plane), NULL, false);
    if (!pl || pl->vertexSize <= 0) { err = "prefabbandera: NewMesh fallo"; return false; }
    W3dMallaCrearRecurso(pl, "BanderaMalla");
    ReemplazarObjetoRaiz(pl, "Bandera");
    VertexAnimation* va = new VertexAnimation(pl, "ondear", false);
    va->startFrame = 1; va->endFrame = 10;
    std::vector<GLfloat> base(pl->vertex, pl->vertex + pl->vertexSize * 3);
    std::vector<GLfloat> alto = base;
    for (int i = 0; i < pl->vertexSize; i++) alto[(size_t)i * 3 + 1] += 1.0f;
    VertexAnimSetKey(*va, 1, &base[0], NULL, NULL, KfLinear);
    VertexAnimSetKey(*va, 10, &alto[0], NULL, NULL, KfLinear);
    NewActiveVertexAnimation(pl, va);
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "prefabbandera: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [prefabbandera] prefab 'Bandera': plano de %d vertices con la vertex anim 'ondear' (%d keyframes)\n",
           pl->vertexSize, (int)va->frames.size());
    return true;
}

// la primera vertex anim de lo generado (o del arbol 'o')
VertexAnimation* PrimeraAnim(Object* o, Mesh** malla) {
    Mesh* m = HijoTipo<Mesh>(o, ObjectType::mesh);
    if (malla) *malla = m;
    return (m && !m->animations.empty()) ? m->animations[0] : NULL;
}
bool CmdPrefabFrames(std::istringstream& ss, std::string& err) {
    std::string p, k; ss >> p;
    std::vector<InstanciaPrefab*> v;
    W3dPrefabInstancias(p, v);
    std::set<const void*> juegos;
    for (size_t i = 0; i < v.size(); i++) {
        VertexAnimation* a = PrimeraAnim(v[i], NULL);
        if (!a || a->frames.empty()) { err = "prefabframes: '" + v[i]->name + "' no genero una vertex anim con frames"; return false; }
        juegos.insert((const void*)a->frames[0]);
    }
    // la raiz CARGADA del prefab (si esta): sus frames
    const int idx = W3dRaizBuscar(W3D_RAIZ_PREFAB, p);
    Object* praiz = (idx >= 0) ? W3dRaices()[(size_t)idx].raiz : NULL;
    VertexAnimation* ta = praiz ? PrimeraAnim(praiz, NULL) : NULL;
    const bool conPlantilla = ta && !ta->frames.empty() && juegos.size() == 1 && juegos.count((const void*)ta->frames[0]);
    printf("      [prefabframes] %s: %d instancia(s), %d juego(s) de frames%s\n", p.c_str(), (int)v.size(), (int)juegos.size(),
           !ta ? " (la raiz del prefab no esta cargada)" : conPlantilla ? " (los mismos que la plantilla)" : " (distintos de la plantilla)");
    while (ss >> k) {
        int val = -1; if (!(ss >> val)) { err = "prefabframes: falta el valor de '" + k + "'"; return false; }
        if (k == "instancias" && (int)v.size() != val) { err = "prefabframes: hay " + Ent((long)v.size()) + " instancias"; return false; }
        else if (k == "juegos" && (int)juegos.size() != val) { err = "prefabframes: hay " + Ent((long)juegos.size()) + " juegos de frames"; return false; }
        else if (k == "plantilla" && (conPlantilla ? 1 : 0) != val) { err = "prefabframes: plantilla=" + Ent(conPlantilla ? 1 : 0); return false; }
        else if (k != "instancias" && k != "juegos" && k != "plantilla") { err = "prefabframes: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// un keyframe nuevo en la vertex anim de la PLANTILLA (la raiz cargada del prefab), sin abrirla: la edicion
// pasa por el COW de siempre (VertexAnimSetKey -> DesinstanciarFrames)
bool CmdPrefabVaKey(std::istringstream& ss, std::string& err) {
    std::string p; int frame = 5; ss >> p >> frame;
    const int idx = W3dRaizBuscar(W3D_RAIZ_PREFAB, p);
    Object* praiz = (idx >= 0) ? W3dRaices()[(size_t)idx].raiz : NULL;
    Mesh* m = NULL;
    VertexAnimation* a = praiz ? PrimeraAnim(praiz, &m) : NULL;
    if (!a || !m || !m->vertex) { err = "prefabvakey: la raiz de '" + p + "' no esta cargada o no tiene vertex anims"; return false; }
    std::vector<GLfloat> pose(m->vertex, m->vertex + m->vertexSize * 3);
    const int k = VertexAnimSetKey(*a, frame, &pose[0], NULL, NULL, KfLinear);
    printf("      [prefabvakey] %s: keyframe %d en el %d (la plantilla tiene %d keyframes, %s)\n", p.c_str(), k, frame,
           (int)a->frames.size(), a->datosRec ? "compartidos" : "propios");
    if (k < 0) { err = "prefabvakey: no se pudo insertar el keyframe"; return false; }
    return true;
}

bool CmdPrefabVaPose(std::istringstream& ss, std::string& err) {
    std::string a1, a2; ss >> a1 >> a2;
    InstanciaPrefab* i1 = Inst(a1, err, "prefabvapose"); if (!i1) return false;
    InstanciaPrefab* i2 = Inst(a2, err, "prefabvapose"); if (!i2) return false;
    Mesh* m1 = NULL; Mesh* m2 = NULL;
    VertexAnimation* va1 = PrimeraAnim(i1, &m1);
    VertexAnimation* va2 = PrimeraAnim(i2, &m2);
    if (!va1 || !va2 || va1->frames.empty() || va2->frames.empty()) { err = "prefabvapose: sin vertex anims generadas"; return false; }
    // (B primero: posar A despues no la puede mover)
    EvalVertexAnim(*va2, m2, 1.0f);
    EvalVertexAnim(*va1, m1, 10.0f);
    double d = 0.0, reposo = 0.0;
    const GLfloat* f1 = va2->frames[0]->positions;
    for (int i = 0; i < m1->vertexSize * 3 && i < m2->vertexSize * 3; i++) {
        d += fabs(m1->vertex[i] - m2->vertex[i]);
        if (f1) reposo += fabs(m2->vertex[i] - f1[i]);
    }
    printf("      [prefabvapose] %s (frame 10) vs %s (frame 1): diferencia %.4f, %s fuera de reposo %.4f\n", a1.c_str(), a2.c_str(), d, a2.c_str(), reposo);
    EvalVertexAnim(*va1, m1, 1.0f);
    if (m1->vertex == m2->vertex || d < 1e-3) { err = "prefabvapose: las dos instancias tienen la misma pose"; return false; }
    if (reposo > 1e-4) { err = "prefabvapose: posar una instancia movio a la otra"; return false; }
    return true;
}

bool CmdPrefabPila(std::istringstream& ss, std::string& err) {
    (void)ss;
    // el PISO (en la raiz activa): una caja estatica cuya cara de arriba es y = 0
    Mesh* piso = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    if (!piso) { err = "prefabpila: NewMesh fallo"; return false; }
    piso->SetNameObj("Piso");
    W3dMallaCrearRecurso(piso, "PisoMalla");
    piso->pos = Vector3(0, -1, 0);
    piso->scale = Vector3(30, 1, 30);
    W3dRigidoDef* dp = new W3dRigidoDef();
    dp->tipo = 0;
    for (int q = 0; q < 3; q++) { dp->caja[q] = 2.0f; dp->centro[q] = 0.0f; }
    piso->fisica = dp;
    // el PREFAB: Pila (vacio) -> Base (vacio) -> Bloque (cubo con cuerpo dinamico)
    const int idx = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Pila");
    if (idx < 0 || SceneCollection->Childrens.empty()) { err = "prefabpila: no se pudo crear el prefab"; return false; }
    Object* raiz = SceneCollection->Childrens[0];
    Empty* base = new Empty(raiz, Vector3(1, 1, 0));
    base->SetNameObj("Base");
    Mesh* b = (Mesh*)NewMesh(MeshType(MeshType::cube), base, false);
    if (!b) { err = "prefabpila: NewMesh (bloque) fallo"; return false; }
    b->SetNameObj("Bloque");
    b->pos = Vector3(0, 0, 0);
    W3dMallaCrearRecurso(b, "BloqueMalla");
    W3dRigidoDef* d = new W3dRigidoDef();
    d->tipo = 1; d->masa = 1.0f;
    for (int q = 0; q < 3; q++) { d->caja[q] = 2.0f; d->centro[q] = 0.0f; }
    b->fisica = d;
    Script(b, "caja.lua");
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "prefabpila: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [prefabpila] Piso estatico + prefab 'Pila' (Pila > Base > Bloque con fisica)\n");
    return true;
}

bool CmdPrefabRigido(std::istringstream& ss, std::string& err) {
    std::string n, ruta, k; ss >> n >> ruta;
    InstanciaPrefab* ip = Inst(n, err, "prefabrigido"); if (!ip) return false;
    Object* o = Nodo(ip, ruta);
    if (!o) { err = "prefabrigido: no hay nada en '" + ruta + "'"; return false; }
    int cual = -1;
    Vector3 centro, semi, ejes[3];
    for (int i = 0; i < W3dRigidosCantidad() && cual < 0; i++) {
        Object* c = NULL;
        if (W3dRigidoCaja(i, &c, &centro, ejes, &semi, NULL, NULL) && c == o) cual = i;
    }
    if (cual >= 0) printf("      [prefabrigido] %s:%s cuerpo #%d centro (%.3f, %.3f, %.3f) semiejes (%.3f, %.3f, %.3f)\n", n.c_str(), ruta.c_str(),
                          cual, centro.x, centro.y, centro.z, semi.x, semi.y, semi.z);
    else printf("      [prefabrigido] %s:%s sin cuerpo\n", n.c_str(), ruta.c_str());
    while (ss >> k) {
        if (k == "cuerpo") { int v = 0; ss >> v; if ((cual >= 0 ? 1 : 0) != v) { err = std::string("prefabrigido: ") + (cual >= 0 ? "tiene" : "no tiene") + " cuerpo"; return false; } continue; }
        if (cual < 0) { err = "prefabrigido: '" + ruta + "' no tiene cuerpo"; return false; }
        if (k == "semi") { float v = 0; ss >> v; if (!Cerca(semi.x, v)) { err = "prefabrigido: el semieje x es " + Ent((long)(semi.x * 1000)) + "/1000"; return false; } continue; }
        if (k == "centro") {
            std::string eje, op; float v = 0; ss >> eje >> op >> v;
            const float c = (eje == "x") ? centro.x : (eje == "y") ? centro.y : centro.z;
            if ((op == "menor" && !(c < v)) || (op == "mayor" && !(c > v))) { err = "prefabrigido: el centro en " + eje + " no es " + op; return false; }
            continue;
        }
        err = "prefabrigido: no entiendo '" + k + "'"; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  el juego compilado
// ---------------------------------------------------------------------------
bool CmdJuegoPrefabsMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "juegoprefabsmin: uso: juegoprefabsmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("prefabenemigo " + dir, e2)) { err = "juegoprefabsmin: " + e2; return false; }
    InteractionMode = ObjectMode; estado = editNavegacion;
    for (int i = 0; i < 3; i++) {
        std::string motivo;
        InstanciaPrefab* ip = W3dPrefabAgregar("Enemigo", Vector3(-4.0f + 4.0f * (float)i, 0.0f, -6.0f), &motivo);
        if (!ip) { err = "juegoprefabsmin: " + motivo; return false; }
        if (i == 1) {
            // el del medio con otra vida: el override de la propiedad (como la tarjeta Scripts de lo generado)
            W3dPrefabOverrideProp(ip, "vida", "25");
        }
    }
    Empty* dirObj = new Empty(NULL, Vector3(0, -10, 0));
    dirObj->SetNameObj("Director");
    Script(dirObj, "director.lua");
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [juegoprefabsmin] prefabenemigo + 3 instancias (Enemigo, Enemigo.001 con vida=25, Enemigo.002) + Director en '%s'\n", dir.c_str());
    return true;
}

bool CmdJuegoPrefabsLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre; ss >> dir >> nombre;
    if (dir.empty() || nombre.empty()) { err = "juegoprefabslog: uso: juegoprefabslog <carpeta> <nombre>"; return false; }
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "juegoprefabslog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "juegoprefabslog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string veredicto;
    int generados = 0;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[prefabs] ");
        if (p && (strstr(p, "OK") || strstr(p, "FALTA")) && strstr(p, "creados=")) veredicto = p;
    }
    fclose(f);
    (void)generados;
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [juegoprefabslog] salida=%d | %s\n", r, veredicto.empty() ? "(sin veredicto del director)" : veredicto.c_str());
    if (veredicto.find("[prefabs] OK") == std::string::npos) {
        err = "juegoprefabslog: en el juego compilado instanciar()/destruir() no dieron lo mismo que en el Play";
        return false;
    }
    std::string k;
    while (ss >> k) if (veredicto.find(k) == std::string::npos) { err = "juegoprefabslog: el veredicto no dice '" + k + "'"; return false; }
    return true;
}

} // namespace

// ============================================================================
//  el despachador
// ============================================================================
bool W3dPruebasPrefabsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "prefabenemigo")       return CmdPrefabEnemigo(ss, err);
    if (cmd == "prefabinstanciar")    return CmdPrefabInstanciar(ss, err);
    if (cmd == "prefabinfo")          return CmdPrefabInfo(ss, err);
    if (cmd == "prefabnodo")          return CmdPrefabNodo(ss, err);
    if (cmd == "prefabscope")         return CmdPrefabScope(ss, err);
    if (cmd == "prefabskin")          return CmdPrefabSkin(ss, err);
    if (cmd == "prefabcompartido")    return CmdPrefabCompartido(ss, err);
    if (cmd == "prefabclip")          return CmdPrefabClip(ss, err);
    if (cmd == "prefabpose")          return CmdPrefabPose(ss, err);
    if (cmd == "prefabregenerar")     return CmdPrefabRegenerar(ss, err);
    if (cmd == "prefabcontar")        return CmdPrefabContar(ss, err);
    if (cmd == "prefabcrear")         return CmdPrefabCrear(ss, err);
    if (cmd == "prefabdesempaquetar") return CmdPrefabDesempaquetar(ss, err);
    if (cmd == "prefabcambiar")       return CmdPrefabCambiar(ss, err);
    if (cmd == "prefabreset")         return CmdPrefabReset(ss, err);
    if (cmd == "prefabh")             return CmdPrefabH(ss, err);
    if (cmd == "prefaborigen")        return CmdPrefabOrigen(ss, err);
    if (cmd == "prefabkey")           return CmdPrefabKey(ss, err);
    if (cmd == "prefabadd")           return CmdPrefabAdd(ss, err);
    if (cmd == "prefabmenu")          return CmdPrefabMenu(ss, err);
    if (cmd == "escenamenu")          return CmdEscenaMenu(ss, err);
    if (cmd == "escenamenuelegir")    return CmdEscenaMenuElegir(ss, err);
    if (cmd == "prefabmenuelegir")    return CmdPrefabMenuElegir(ss, err);
    if (cmd == "prefaboverride")      return CmdPrefabOverride(ss, err);
    if (cmd == "prefabvivos")         return CmdPrefabVivos(ss, err);
    if (cmd == "prefabmundo")         return CmdPrefabMundo(ss, err);
    if (cmd == "prefabdef")           return CmdPrefabDef(ss, err);
    if (cmd == "prefabsel")           return CmdPrefabSel(ss, err);
    if (cmd == "simcache") {   // simcache 0|1: el cache del rewind del Play (apagado, el heap no crece por tick)
        extern bool gSimCacheOn;
        int v = 1; ss >> v;
        gSimCacheOn = (v != 0);
        printf("      [simcache] %s\n", gSimCacheOn ? "prendido" : "apagado");
        return true;
    }
    if (cmd == "prefabreparent")      return CmdPrefabReparent(ss, err);
    if (cmd == "prefabjer")           return CmdPrefabJer(ss, err);
    if (cmd == "juegoprefabsmin")     return CmdJuegoPrefabsMin(ss, err);
    if (cmd == "prefabelegir")        return CmdPrefabElegir(ss, err);
    if (cmd == "prefabobj")           return CmdPrefabObj(ss, err);
    if (cmd == "prefabbandera")       return CmdPrefabBandera(ss, err);
    if (cmd == "prefabframes")        return CmdPrefabFrames(ss, err);
    if (cmd == "prefabvapose")        return CmdPrefabVaPose(ss, err);
    if (cmd == "prefabvakey")         return CmdPrefabVaKey(ss, err);
    if (cmd == "prefabpila")          return CmdPrefabPila(ss, err);
    if (cmd == "prefabrigido")        return CmdPrefabRigido(ss, err);
    if (cmd == "juegoprefabslog")     return CmdJuegoPrefabsLog(ss, err);
    manejado = false;
    return false;
}
