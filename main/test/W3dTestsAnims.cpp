// ============================================================================
//  W3dTestsAnims.cpp — comandos de harness de los ANIMSETS (ver W3dTestsAnims.h
//  y animation/W3dAnimSet.h). Compila en segundos: los comandos de animsets van aca.
//
//  Comandos:
//    animsetinfo [vivos N] [refs N] [clips N] [variantes N] [clipsvariante N] [copias N] [registro N]
//                [descriptores N] [enmemoria N]
//        los animsets cargados en el almacen (recursos LISTO), cuantos armatures los usan,
//        clips y keyframes en memoria, variantes (esqueletos que no calzan), cuantas copias
//        (copy-on-write) hubo desde el arranque y el registro del proyecto. 'descriptores' =
//        TODOS los descriptores ANIMSET del almacen, en cualquier estado (un FALLO que nadie
//        referencia no puede quedar para siempre); 'enmemoria' = los animsets que todavia no
//        tienen entrada (los del Alt+D de un armature con clips propios, "#memoria...").
//    animsetrec <id> [refs N] [nada]
//        el recurso ANIMSET con ese id (su entrada): estado y referencias; 'nada' asserta que no
//        hay (el guardado desengancha el recurso cuya entrada cambio).
//    animsetarm <armature> [compartido 0|1] [animset X] [clips N] [variante 0|1] [nocargo 0|1] [nombreset X]
//        un armature: si sus clips son del recurso o propios, de que animset y cuantos.
//        'nombreset' = su animSetName (la biblioteca a la que lo manda el guardado).
//    animsetclip <armature> <i> [nombre X] [keys N] [mismo <armature2> <j> 0|1]
//        el clip i de un armature; 'mismo' asserta si es EL MISMO objeto en memoria que el
//        clip j de otro (compartido de verdad) o no.
//    animsetsw3d <ruta.w3d> [animsets N] [clips N] [inline N] [armatures N] [jsonmaxkb N] [bytesmaxkb N]
//        lo que hay ADENTRO de un contenedor: las entradas .w3da (cada una tiene que leerse
//        entera), sus clips, el registro, los armatures que nombran animset y los que traen
//        los clips INLINE (el formato de antes), y el tamano de project.json.
//    animsinline 0|1
//        el guardado escribe los clips INLINE como antes (para fabricar archivos "viejos").
//    animsetsintetica <n> [ruta.w3d] [invertido] [agregar]
//        escena NUEVA con n armatures del MISMO rig (5 huesos), cada uno con 4 clips; el
//        segundo con un "Quieto" distinto y el tercero con un clip repetido. Los clips cubren
//        todos los modos del formato (horneados, bezier con handles, frames irregulares,
//        interpolaciones mezcladas, -0.0, una pista sin hueso y una repetida). 'invertido'
//        agrega uno mas con los huesos en OTRO orden (mismos nombres). Con ruta la guarda.
//        'addui' los mete en la escena que ya esta (sin cerrarla y sin "SinClips").
//    animsetluajuego <carpeta>
//        reescribe el juego3dmin.lua de 'game3dmin' para que la franja del HUD se prenda SOLO si
//        el armature "Personaje" tiene sus clips (animClip/animLength): el binario compilado que
//        no cargue el animset .w3da no pasa juego3dpx (prueba_juego3d_anims.w3s).
//    animsetjuegoquitar <carpeta> quitar|volver
//        saca / devuelve la carpeta animations/ de los assets del juego YA compilado
//        (<carpeta>/build/linux/animaciones): sin ella el mismo binario no ve los clips y
//        juego3dpx TIENE que fallar (la prueba del juego se prueba a si misma).
//    animsetpistas <ruta.w3d> <entrada.w3da> [<hueso> N]...
//        las pistas de los clips de UNA entrada .w3da contadas por el NOMBRE de su hueso en la
//        tabla HUES ("-" = sin hueso): cuantas apuntan a cada hueso (un hueso que le falta al
//        esqueleto de una VARIANTE no puede perder sus pistas al guardar).
//    animsetapuntar <origen.w3d> <destino.w3d> <armature> <animset>
//        copia el contenedor reescribiendo su project.json para que ese armature use OTRO animset
//        (mismos indices de clips): asi se fabrica un armature cuyo esqueleto no calza con
//        la tabla del animset (una VARIANTE) por el camino de carga de verdad.
//    animsetrenombrar <armature> <i> <nombre>
//        renombra el clip i como lo hace el editor (copy-on-write primero).
//    animsetborrar <objeto>
//        borra un objeto (con su subarbol) de la escena, como la X del outliner.
//    animsetactivo <armature>
//        lo deja como objeto ACTIVO (el que usan 'anim' / 'animkey' de W3dScript.cpp).
//    animsetrenombrarhueso <armature> <hueso> <nuevo>
//        renombra un hueso SIN tocar los clips (como el rename de un vertex group que arrastra al
//        hueso homonimo): el guardado no lo puede dejar en su animset por nombre.
//    animsetborrarhueso <armature> <hueso>
//        borra ese hueso como la X del Edit Mode de huesos (BoneEditDelete: remapea las pistas).
//    posefoto <archivo> [juego 0|1] [sin <armature>]...
//        la FOTO de la reproduccion: por cada armature, cada clip y varios frames (inicio,
//        siguiente, medio, final-1, final, pasado el final; con 'game 1' ademas el cabezal
//        CONTINUO del juego en fracciones de frame), el hash de TODO lo que sale de la pose:
//        poseT/R/S, poseWorld, skinMatrix, poseHead y poseTail de cada hueso (los bits).
//        'sin X' deja afuera a ese armature y 'solo X' fotografia SOLO a los nombrados (a los
//        DEMAS despues de editar uno, o al editado).
//    posefotoigual <a> <b>
//        dos fotos IDENTICAS (misma lista de poses, mismos bits).
//    animsetromper <origen.w3d> <destino.w3d> corte|version|req|falta
//        copia el contenedor con su primera .w3da rota (cortada / version nueva / pide un
//        bloque desconocido / borrada): el armature no carga sus clips y guardar se frena.
//    animsetregistro
//        los fourcc que entiende el codigo = las filas "w3da:" vivas de formato/bloques.tsv, y
//        cada uno documentado en formato/w3da.md (sin formato/: "SIN CORPUS:", se saltea).
//    animsetcorrupto
//        robustez del lector .w3da: todos los cortes, bytes al azar, version nueva, bloque
//        requerido desconocido (rechazo) y bloque opcional desconocido (se preserva y se
//        reescribe igual). Ninguno puede caerse ni dejar clips a medias.
//    animsetsellos [off|on] [clips N]  |  animsetsellos prueba <armature>
//        los SELLOS de los clips compartidos (ver "SELLOS" abajo): se verifican solos antes de
//        CADA comando; este los verifica ya (el ultimo comando de un script no tiene "siguiente")
//        y cuenta los clips sellados. 'off' los apaga (medir), 'on' los prende. 'prueba' prueba
//        el DETECTOR: escribe un clip compartido sin copiar, exige que se vea y lo restaura.
//    animsetver <armature> <clip> [compartido 0|1] [copias N]
//        MIRAR el clip como el usuario (Pose Mode, huesos elegidos, clip en el timeline): dope
//        sheet, editor de curvas (encuadre, trazo, click en un keyframe), la tarjeta Keyframe,
//        la UI entera dibujada y la reproduccion. 'copias' = copy-on-write de ESTE comando.
//    animsetdope <armature> <clip> <op> [compartido 0|1] [copias N]
//        UNA edicion del clip por el camino real de la UI. op: mover | escalar | cancelar |
//        borrar | duplicar | interp | handletipo | euler | kfvalor (tarjeta Keyframe) |
//        handle (arrastrar un handle en curvas) | insertar (la I de Pose Mode) | fin / finigual
//        (el End del clip: cambiado / confirmado igual) | renombrar / renombrararm (los botones
//        Rename de la tarjeta Animacion y de la del armature: rename in-place + Enter) | undo (Ctrl+Z).
//    animsetusuarios <animset> [N]
//    animsetrenombrarset <viejo> <nuevo> [<esperado>]
//    animsetcarpeta <animset> fijar|es <carpeta>
//    animsetpurgar [N]
//        los animsets como RECURSOS del proyecto (io/SaveAnimSets.h, lo que usa el outliner
//        por recursos): usuarios, renombrar, carpeta cosmetica, purgar los huerfanos.
//  CLAVES COMPACTAS (W3dKeys, animation/Animation.h: una curva horneada de un .w3da en ~4 B por clave):
//    clavessintetica [ruta.w3d] [invertido]
//        escena NUEVA con 2 armatures "Horneado" (el rig de animsetsintetica) con los MISMOS clips
//        "Largo" (22 curvas que se compactan: bezier HAuto/HAutoClamped/HVector/HFree en cero, lineal,
//        escalon, paso 1/2/3/5, primer frame negativo, frames enormes, un -0.0, un tramo plano; y 3 que
//        no: un key, frames irregulares, un handle guardado) y "Corto" (16, de 2 y 3 claves).
//        'invertido' agrega "Invertido": los huesos en otro orden (al abrir, los clips por una VARIANTE).
//    clavesinfo [marca] [compactas N] [nuevas N] [maxbpc X]
//        las claves de TODAS las curvas: compactas vivas, bytes, cuantas se expandieron. 'marca' recuerda
//        las expansiones de ahora; 'nuevas' = las expandidas desde la marca; 'maxbpc' = B por clave compacta.
//    clavescomparar <armature> [paso X] [compactas N]
//        cada curva de cada clip: la curva (compacta o no) y una COMUN con sus mismas claves contra la
//        evaluacion de ANTES (copiada aca): EvalF en todos los frames enteros del rango +-3, fracciones de
//        'paso' (0.125), una al azar por frame, +-1e30 y NaN; Eval(int) y EffectiveHandle de cada clave. Todo
//        bit a bit, y mirar no puede expandir nada.
//    claveseditar <armature> <clip> insertar|mover|borrar
//        edita la primera curva compacta del clip como el editor (copy-on-write, undo de keyframes): se
//        expande SOLO esa, la edicion se ve, Ctrl+Z devuelve las mismas claves (compactas) y Ctrl+Y la rehace.
// ============================================================================
#include "test/W3dTestsAnims.h"
#include "animation/W3dAnimSet.h"
#include "animation/SkeletalAnimation.h"
#include "animation/Animation.h"
#include "objects/Armature.h"
#include "objects/Objects.h"
#include "objects/Collection.h"
#include "io/W3dResources.h"
#include "io/W3dZip.h"
#include "io/JsonW3d.h"
#include "io/SaveAnimSets.h"
#include "edit/BoneEdit.h"            // animsetborrarhueso: BoneEditDelete (remapea las pistas de los clips)
#include "w3dFilesystem.h"
#include "base/W3dInteractionState.h"  // animsetver/animsetdope: Pose Mode (InteractionMode / estado)
#include "ViewPorts/Timeline.h"        // animsetver/animsetdope: el dope sheet y el editor de curvas de verdad
#include "ViewPorts/ViewPorts.h"       // animsetver: la UI entera dibujada (rootViewport)
#include "undo/Undo.h"                 // animsetdope: el undo de keyframes (tarjeta Keyframe, Ctrl+Z)
#include "test/W3dScript.h"            // animsetver: W3dRunCommand("propstab 9") refresca la tarjeta Keyframe
#include "ViewPorts/Properties.h"      // animsetdope renombrar: los botones Rename de las tarjetas (PropsActive)
#include "WhiskUI/widgets/TextField.h" // animsetdope renombrar: el campo in-place del rename (g_activeTextField)

#include <map>
#include <set>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>                  // mkdir de ../debug (las fotos de pose)
#ifdef _WIN32
#include <direct.h>                    // _mkdir
#endif

extern void RestartScene();
extern void LoadPendingTextures();   // animsetver: la UI entera se dibuja con sus texturas
extern void RenameCommit();          // animsetdope renombrar: el Enter del rename in-place (Properties.cpp)
extern bool SaveW3D(const std::string& path);
extern Object* FindObjectByName(Object* node, const std::string& name);

namespace {

// el resto de la linea como ruta/getName (con comillas si tiene espacios)
std::string ReadWord(std::istringstream& ss) {
    std::string t;
    if (!(ss >> t)) return std::string();
    if (t.empty() || t[0] != '"') return t;
    std::string r = t.substr(1);
    while (r.empty() || r[r.size() - 1] != '"') {
        std::string more;
        if (!(ss >> more)) break;
        r += " " + more;
    }
    if (!r.empty() && r[r.size() - 1] == '"') r.erase(r.size() - 1);
    return r;
}

// un nombre SIN carpeta va a ../debug/ (la carpeta de salidas de la suite, ignorada por git),
// igual que W3dDebugFile de W3dScript.cpp; una ruta con carpeta queda como esta
std::string DebugPath(const std::string& n) {
    if (n.find('/') != std::string::npos || n.find('\\') != std::string::npos) return n;
#ifdef _WIN32
    _mkdir("../debug");
#else
    mkdir("../debug", 0755);
#endif
    return "../debug/" + n;
}

// pares "clave valor" del final de la linea (los asserts)
bool ReadAsserts(std::istringstream& ss, std::vector<std::string>& keys, std::vector<std::string>& vals,
                 const char* cmd, std::string& err) {
    std::string k;
    while (ss >> k) {
        std::string v;
        if (!(ss >> v)) { err = std::string(cmd) + ": falta el valor de '" + k + "'"; return false; }
        keys.push_back(k); vals.push_back(v);
    }
    return true;
}

bool AssertInt(const char* cmd, const std::string& k, const std::string& v, long has, std::string& err) {
    const long wants = atol(v.c_str());
    if (wants == has) return true;
    char b[200];
    sprintf(b, "%s: %s = %ld, se esperaba %ld", cmd, k.c_str(), has, wants);
    err = b;
    return false;
}

Armature* ArmByName(const std::string& n) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : 0;
    return (o && o->getType() == ObjectType::armature) ? (Armature*)o : 0;
}

void CollectArms(Object* o, std::vector<Armature*>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) out.push_back((Armature*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectArms(o->Childrens[i], out);
}

// ============================================================================
//  animsetinfo
// ============================================================================
bool CmdInfo(std::istringstream& ss, std::string& err) {
    W3dAnimSetsStats st;
    W3dAnimSetsGetStats(st);
    const std::vector<W3dAnimSetRow>& reg = W3dAnimSetsRegistry();
    std::vector<W3dResource*> descs;
    W3dResourcesList(W3DREC_ANIMSET, descs);
    int inMemory = 0;
    for (size_t i = 0; i < descs.size(); i++)
        if (descs[i] && descs[i]->id.compare(0, 8, "#memoria") == 0) inMemory++;
    printf("      [animsetinfo] vivos=%d refs=%d clips=%d variantes=%d clipsVariante=%d keys=%ld KB=%ld copias=%d registro=%d descriptores=%d enmemoria=%d\n",
           st.live, st.refs, st.clips, st.variants, st.clipsVariant, st.keys, st.bytes / 1024,
           W3dAnimSetsCopies(), (int)reg.size(), (int)descs.size(), inMemory);
    for (size_t i = 0; i < descs.size() && i < 40; i++)
        if (descs[i] && (descs[i]->state != W3DREC_READY || descs[i]->id.empty() || descs[i]->id[0] == '#'))
            printf("      [animsetinfo]   descriptor '%s' estado=%d refs=%d\n", descs[i]->id.c_str(),
                   (int)descs[i]->state, descs[i]->refTotal);
    for (size_t i = 0; i < reg.size() && i < 40; i++)
        printf("      [animsetinfo]   '%s' -> %s%s%s\n", reg[i].name.c_str(), reg[i].entry.c_str(),
               reg[i].folder.empty() ? "" : " folder=", reg[i].folder.c_str());
    std::vector<std::string> k, v;
    if (!ReadAsserts(ss, k, v, "animsetinfo", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        long has = 0;
        if      (k[i] == "live")         has = st.live;
        else if (k[i] == "refs")          has = st.refs;
        else if (k[i] == "clips")         has = st.clips;
        else if (k[i] == "variants")     has = st.variants;
        else if (k[i] == "clipsvariant") has = st.clipsVariant;
        else if (k[i] == "copies")        has = W3dAnimSetsCopies();
        else if (k[i] == "registry")      has = (long)reg.size();
        else if (k[i] == "descriptores")  has = (long)descs.size();
        else if (k[i] == "inmemory")     has = inMemory;
        else { err = "animsetinfo: assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt("animsetinfo", k[i], v[i], has, err)) return false;
    }
    return true;
}

// ============================================================================
//  animsetrec <id> [refs N] [nada]: el descriptor del almacen con ese id (la entrada)
// ============================================================================
bool CmdRec(std::istringstream& ss, std::string& err) {
    const std::string id = ReadWord(ss);
    W3dResource* r = W3dResourceFind(W3DREC_ANIMSET, id);
    const W3dAnimSet* set = (r && r->state == W3DREC_READY) ? (const W3dAnimSet*)r->data : 0;
    if (r) printf("      [animsetrec] '%s': estado=%d refs=%d animset='%s'\n", id.c_str(), (int)r->state, (int)r->refTotal,
                  set ? set->name.c_str() : "");
    else   printf("      [animsetrec] '%s': NADA (ningun recurso con ese id)\n", id.c_str());
    std::string k;
    while (ss >> k) {
        if (k == "none") { if (r) { err = "animsetrec: hay un recurso con id '" + id + "'"; return false; } continue; }
        if (k == "refs") {
            long v = -1; ss >> v;
            if (!r || (long)r->refTotal != v) { char b[160]; sprintf(b, "animsetrec: refs = %d, se esperaba %ld", r ? (int)r->refTotal : -1, v); err = b; return false; }
            continue;
        }
        err = "animsetrec: assert desconocido '" + k + "'"; return false;
    }
    return true;
}

// ============================================================================
//  animsetarm / animsetclip
// ============================================================================
bool CmdArm(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    Armature* a = ArmByName(n);
    if (!a) { err = "animsetarm: no hay un armature '" + n + "'"; return false; }
    W3dAnimSet* set = W3dArmatureAnimSet(a);
    printf("      [animsetarm] '%s': %d clips %s%s%s%s animSetNombre='%s'%s\n", a->name.c_str(), (int)a->animations.size(),
           set ? "COMPARTIDOS de '" : "PROPIOS", set ? set->name.c_str() : "", set ? "'" : "",
           a->animsVar ? " (variante)" : "", a->animSetName.c_str(),
           a->animSetNotLoaded.empty() ? "" : " NO CARGO");
    std::vector<std::string> k, v;
    if (!ReadAsserts(ss, k, v, "animsetarm", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        if (k[i] == "animset") {
            const std::string has = set ? set->name : std::string();
            if (has != v[i]) { err = "animsetarm: animset = '" + has + "', se esperaba '" + v[i] + "'"; return false; }
            continue;
        }
        if (k[i] == "setname") {
            if (a->animSetName != v[i]) { err = "animsetarm: animSetNombre = '" + a->animSetName + "', se esperaba '" + v[i] + "'"; return false; }
            continue;
        }
        long has = 0;
        if      (k[i] == "shared") has = set ? 1 : 0;
        else if (k[i] == "clips")      has = (long)a->animations.size();
        else if (k[i] == "variant")   has = a->animsVar ? 1 : 0;
        else if (k[i] == "notloaded")    has = a->animSetNotLoaded.empty() ? 0 : 1;
        else { err = "animsetarm: assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt("animsetarm", k[i], v[i], has, err)) return false;
    }
    return true;
}

long KeysOf(const SkeletalAnimation* c) {
    long k = 0;
    if (c) W3dAnimSetClipBytes(c, &k);
    return k;
}

bool CmdClip(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    int i = -1; ss >> i;
    Armature* a = ArmByName(n);
    if (!a) { err = "animsetclip: no hay un armature '" + n + "'"; return false; }
    if (i < 0 || i >= (int)a->animations.size() || !a->animations[i]) { err = "animsetclip: el clip no existe"; return false; }
    const SkeletalAnimation* c = a->animations[i];
    printf("      [animsetclip] '%s'[%d] = '%s' fps=%d %d..%d pistas=%d keys=%ld\n", a->name.c_str(), i, c->name.c_str(),
           c->FrameRate, c->startFrame, c->endFrame, (int)c->tracks.size(), KeysOf(c));
    std::string k;
    while (ss >> k) {
        if (k == "name") {
            const std::string v = ReadWord(ss);
            if (c->name != v) { err = "animsetclip: nombre = '" + c->name + "', se esperaba '" + v + "'"; return false; }
        } else if (k == "keys") {
            long v = -1; ss >> v;
            if (KeysOf(c) != v) { char b[96]; sprintf(b, "animsetclip: keys = %ld, se esperaba %ld", KeysOf(c), v); err = b; return false; }
        } else if (k == "same") {
            const std::string n2 = ReadWord(ss);
            int j = -1, wants = -1; ss >> j >> wants;
            Armature* b2 = ArmByName(n2);
            if (!b2 || j < 0 || j >= (int)b2->animations.size()) { err = "animsetclip: 'mismo' apunta a un clip que no existe"; return false; }
            const int is = (b2->animations[j] == c) ? 1 : 0;
            printf("      [animsetclip]   mismo objeto que '%s'[%d]: %s\n", b2->name.c_str(), j, is ? "SI" : "no");
            if (is != wants) { err = std::string("animsetclip: 'mismo' = ") + (is ? "1" : "0") + ", se esperaba el otro"; return false; }
        } else { err = "animsetclip: assert desconocido '" + k + "'"; return false; }
    }
    return true;
}

// ============================================================================
//  animsetsw3d: lo que hay adentro de un contenedor
// ============================================================================
void CountArmaturesJson(JVal* o, int& withSet, int& inl) {
    if (!o || o->type != 4) return;
    if (JS(o, "type", "") == "armature") {
        if (!JS(o, "anims", "").empty()) withSet++;
        if (JChild(o, "anims", 5)) inl++;
    }
    JVal* h = JChild(o, "children", 5);
    if (h) for (size_t i = 0; i < h->list.size(); i++) CountArmaturesJson(h->list[i], withSet, inl);
}

bool CmdSw3d(std::istringstream& ss, std::string& err) {
    const std::string path = ReadWord(ss);
    W3dZipReader z;
    if (!z.Open(path)) { err = "animsetsw3d: no puedo abrir " + path; return false; }
    std::vector<std::string> ents;
    z.List(ents);
    std::vector<unsigned char> pj;
    if (!z.Read("project.json", pj) || pj.empty()) { err = "animsetsw3d: sin project.json"; return false; }
    int nSets = 0, nClips = 0, bad = 0;
    long bytesSets = 0;
    std::set<std::string> w3da;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& e = ents[i];
        if (e.size() < 5 || e.compare(e.size() - 5, 5, ".w3da") != 0) continue;
        w3da.insert(e);
        std::vector<unsigned char> d;
        W3dAnimSetData dat;
        std::string reason;
        if (!z.Read(e, d) || d.empty() || !W3dAnimSetRead(&d[0], d.size(), dat, &reason)) {
            printf("      [animsetsw3d] %s NO SE LEE: %s\n", e.c_str(), reason.c_str());
            bad++;
            continue;
        }
        long keys = 0;
        for (size_t c = 0; c < dat.clips.size(); c++) W3dAnimSetClipBytes(dat.clips[c], &keys);
        printf("      [animsetsw3d] %s: %d huesos, %d clips, %ld keys, %d B%s\n", e.c_str(), (int)dat.bones.size(),
               (int)dat.clips.size(), keys, (int)d.size(), dat.foreign.empty() ? "" : " (+bloques ajenos)");
        nSets++; nClips += (int)dat.clips.size(); bytesSets += (long)d.size();
    }
    JParser p((const char*)&pj[0], pj.size());
    JVal* root  = p.Value();
    int withSet = 0, inl = 0, reg = 0, regNoEntry = 0;
    if (root && root->type == 4) {
        JVal* esc = JChild(root, "scene", 4);
        JVal* objs = esc ? JChild(esc, "objects", 5) : 0;
        if (objs) for (size_t i = 0; i < objs->list.size(); i++) CountArmaturesJson(objs->list[i], withSet, inl);
        JVal* ja = JChild(root, "animsets", 5);
        if (ja) for (size_t i = 0; i < ja->list.size(); i++) {
            reg++;
            const std::string in = JS(ja->list[i], "entry", "");
            if (!w3da.count(in)) { regNoEntry++; printf("      [animsetsw3d] el registro nombra '%s' y no esta\n", in.c_str()); }
        }
    }
    delete root;
    printf("      [animsetsw3d] %s: %d animsets (%d clips, %ld KB), registro=%d, armatures con animset=%d, con clips INLINE=%d, project.json=%ld KB\n",
           path.c_str(), nSets, nClips, bytesSets / 1024, reg, withSet, inl, (long)pj.size() / 1024);
    if (bad) { err = "animsetsw3d: hay entradas .w3da que no se leen"; return false; }
    if (regNoEntry) { err = "animsetsw3d: el registro nombra entradas que no estan"; return false; }
    std::vector<std::string> k, v;
    if (!ReadAsserts(ss, k, v, "animsetsw3d", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        if (k[i] == "jsonmaxkb") {
            if ((long)pj.size() > atol(v[i].c_str()) * 1024) { err = "animsetsw3d: project.json pesa mas de " + v[i] + " KB"; return false; }
            continue;
        }
        if (k[i] == "bytesmaxkb") {
            if (bytesSets > atol(v[i].c_str()) * 1024) { err = "animsetsw3d: los .w3da pesan mas de " + v[i] + " KB"; return false; }
            continue;
        }
        long has = 0;
        if      (k[i] == "animsets")  has = nSets;
        else if (k[i] == "clips")     has = nClips;
        else if (k[i] == "inline")    has = inl;
        else if (k[i] == "armatures") has = withSet;
        else if (k[i] == "registry")  has = reg;
        else { err = "animsetsw3d: assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt("animsetsw3d", k[i], v[i], has, err)) return false;
    }
    return true;
}

// ============================================================================
//  animsetsintetica
// ============================================================================
const char* kBones[5]   = { "Raiz", "Cadera", "Torso", "Cabeza", "Brazo" };
const int   kParents[5] = { -1, 0, 1, 2, 2 };

int BoneOf(const Armature* a, const char* n) {
    for (size_t i = 0; i < a->bones.size(); i++) if (a->bones[i].name == n) return (int)i;
    return -1;
}

Armature* CreateRig(Object* parent, const std::string& name, bool inverted, float x) {
    Armature* a = new Armature(parent, Vector3(x, 0, 0));
    a->SetNameObj(name);
    // orden de los huesos: el natural o uno permutado (mismos nombres, otros indices)
    const int orderN[5] = { 0, 1, 2, 3, 4 };
    const int orderI[5] = { 4, 2, 0, 3, 1 };
    const int* order = inverted ? orderI : orderN;
    int indexOf[5];
    for (int k = 0; k < 5; k++) indexOf[order[k]] = k;
    for (int k = 0; k < 5; k++) {
        const int h = order[k];
        W3dBone b;
        b.name = kBones[h];
        b.parent = kParents[h] < 0 ? -1 : indexOf[kParents[h]];
        b.head = Vector3(0.0f, (float)h * 0.5f, 0.0f);
        b.tail = Vector3(0.0f, (float)h * 0.5f + 0.4f, 0.1f * (float)h);
        a->bones.push_back(b);
    }
    PrepareAuthoredSkin(a);
    return a;
}

// un clip determinista que cubre los modos del formato
SkeletalAnimation* SyntheticClip(const Armature* a, const std::string& name, int seed) {
    SkeletalAnimation* c = new SkeletalAnimation(name);
    c->FrameRate = 24 + seed % 7;
    c->startFrame = 1;
    c->endFrame = 30 + seed;
    for (int h = 0; h < 5; h++) {
        BoneTrack tr;
        tr.bone = BoneOf(a, kBones[h]);
        // rotacion HORNEADA: un key por frame, lineal, sin handles (el caso de las anims importadas)
        for (int comp = 0; comp < 3; comp++) {
            AnimProperty ap; ap.Property = AnimRotation; ap.component = comp;
            for (int f = c->startFrame; f <= c->endFrame; f++) {
                keyFrame k; k.frame = f; k.Interpolation = KfLinear; k.handleType = HAuto;
                k.value = (float)(20.0 * sin(0.21 * f + 0.7 * h + 0.3 * comp + 0.11 * seed));
                ap.keyframes.push_back(k);
            }
            tr.Properties.push_back(ap);
        }
        if (h == 0) {
            // posicion de la raiz: BEZIER con handles libres, frames IRREGULARES, interpolaciones mezcladas
            for (int comp = 0; comp < 3; comp++) {
                AnimProperty ap; ap.Property = AnimPosition; ap.component = comp;
                const int fr[6] = { 1, 4, 5, 11, 20, 29 };
                for (int q = 0; q < 6; q++) {
                    keyFrame k; k.frame = fr[q];
                    k.value = (float)(0.37 * q * (comp + 1) + 0.01 * seed);
                    k.Interpolation = (q % 3 == 0) ? KfBezier : (q % 3 == 1 ? KfLinear : KfConstant);
                    k.handleType = (q % 2) ? HFree : HAligned;
                    k.inDF = -1.5f - 0.1f * q; k.inDV = (q == 2) ? -0.0f : 0.25f * q;
                    k.outDF = 1.25f; k.outDV = -0.125f * q;
                    ap.keyframes.push_back(k);
                }
                tr.Properties.push_back(ap);
            }
            // escala: UN key (curva de un solo keyframe)
            AnimProperty is; is.Property = AnimScale; is.component = AnimY;
            keyFrame k; k.frame = 7; k.value = 1.5f; is.keyframes.push_back(k);
            tr.Properties.push_back(is);
            // una curva VACIA (sin keyframes) tambien viaja
            AnimProperty empty; empty.Property = AnimScale; empty.component = AnimZ;
            tr.Properties.push_back(empty);
        }
        c->tracks.push_back(tr);
    }
    // una pista SIN hueso (inerte) y una REPETIDA para la cabeza (el FK usa la primera)
    { BoneTrack tr; tr.bone = -1;
      AnimProperty ap; ap.Property = AnimRotation; ap.component = AnimX;
      keyFrame k; k.frame = 3; k.value = 45.0f; ap.keyframes.push_back(k);
      tr.Properties.push_back(ap); c->tracks.push_back(tr); }
    { BoneTrack tr; tr.bone = BoneOf(a, "Cabeza");
      AnimProperty ap; ap.Property = AnimRotation; ap.component = AnimZ;
      keyFrame k; k.frame = 2; k.value = -90.0f; ap.keyframes.push_back(k);
      tr.Properties.push_back(ap); c->tracks.push_back(tr); }
    return c;
}

bool CmdSynthetic(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 1) { err = "animsetsynthetic: uso: animsetsynthetic <n> [ruta.w3d] [invertido]"; return false; }
    std::string path = ReadWord(ss), extra;
    bool inverted = false, add = false;
    if (path == "inverted") { inverted = true; path.clear(); }
    if (path == "add")   { add = true; path.clear(); }
    while (ss >> extra) { if (extra == "inverted") inverted = true; if (extra == "add") add = true; }
    // 'addui': a la escena que ya esta (sin cerrarla), para meter personajes en otra prueba
    if (!add) RestartScene();
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Personajes");
    if (!add) CollectionActive = col;
    const int total = n + (inverted ? 1 : 0);
    const char* names[4] = { "Quieto", "Caminar", "Correr", "Saltar" };
    for (int i = 0; i < total; i++) {
        const bool inv = (i == n);
        Armature* a = CreateRig(col, inv ? "Invertido" : "Personaje", inv, 2.0f * (float)i);
        for (int c = 0; c < 4; c++) {
            // el segundo personaje tiene su PROPIO "Quieto" (semilla distinta): no se deduplica
            const int seed = (c == 0 && i == 1) ? 50 : c;
            a->animations.push_back(SyntheticClip(a, names[c], seed));
        }
        // el tercero usa DOS VECES el mismo clip (igual contenido y nombre)
        if (i == 2) a->animations.push_back(SyntheticClip(a, names[1], 1));
        a->activeAnim = 0;
    }
    // uno sin clips (no aparece en ningun animset)
    if (!add) { Armature* a = CreateRig(col, "SinClips", false, -2.0f); (void)a; }
    DeselectAll();
    ActiveObj = NULL;
    printf("      [animsetsintetica] %d armatures del mismo rig con 4 clips%s\n", n, inverted ? " + uno con los huesos en otro orden" : "");
    if (!path.empty()) {
        const bool ok = SaveW3D(path);
        const SaveAnimSetsInfo& gi = SaveLastAnimSets();
        printf("      [animsetsintetica] guardada en %s -> %s (animsets=%d clips %d -> %d, %ld B)\n", path.c_str(),
               ok ? "OK" : "FALLO", gi.animsets, gi.clipsArmatures, gi.writtenClips, gi.bytes);
        if (!ok) { err = "animsetsynthetic: no pude guardar " + path; return false; }
    }
    return true;
}

// ============================================================================
//  animsetapuntar: reescribe el "anims" de un armature adentro del contenedor
// ============================================================================
bool CmdPoint(std::istringstream& ss, std::string& err) {
    const std::string path = ReadWord(ss), dest = ReadWord(ss), arm = ReadWord(ss), set = ReadWord(ss);
    if (path.empty() || dest.empty() || arm.empty() || set.empty()) { err = "animsetpoint: uso: animsetpoint <origen.w3d> <destino.w3d> <armature> <animset>"; return false; }
    std::vector<W3dZipEntry> ents;
    if (!W3dZipRead(path, &ents)) { err = "animsetpoint: no puedo leer " + path; return false; }
    bool done = false;
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].name != "project.json") continue;
        std::string t(ents[i].data.begin(), ents[i].data.end());
        const size_t pn = t.find("\"name\": \"" + arm + "\"");
        if (pn == std::string::npos) break;
        const std::string key = "\"anims\": \"";
        const size_t pa = t.find(key, pn);
        if (pa == std::string::npos) break;
        const size_t start = pa + key.size(), end = t.find('"', start);
        if (end == std::string::npos) break;
        t.replace(start, end - start, set);
        ents[i].data.assign(t.begin(), t.end());
        done = true;
    }
    if (!done) { err = "animsetpoint: no encontre el \"anims\" de '" + arm + "'"; return false; }
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].name == "mimetype") w.Add(ents[i].name, ents[i].data);
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].name != "mimetype") w.Add(ents[i].name, ents[i].data);
    if (!w.Save(dest)) { err = "animsetpoint: no pude escribir " + dest; return false; }
    printf("      [animsetapuntar] '%s' ahora usa el animset '%s'\n", arm.c_str(), set.c_str());
    return true;
}

// ============================================================================
//  animsetrenombrar / animsetborrar
// ============================================================================
bool CmdRename(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    int i = -1; ss >> i;
    const std::string newValue = ReadWord(ss);
    Armature* a = ArmByName(n);
    if (!a || i < 0 || i >= (int)a->animations.size() || newValue.empty()) { err = "animsetrename: uso: animsetrename <armature> <i> <nombre>"; return false; }
    W3dOwnArmatureAnims(a);   // lo mismo que hace el rename del panel antes de tomar el puntero
    a->animations[i]->name = newValue;
    printf("      [animsetrenombrar] '%s'[%d] -> '%s'\n", a->name.c_str(), i, newValue.c_str());
    return true;
}

bool CmdLuaGame(std::istringstream& ss, std::string& err) {
    const std::string dir = ReadWord(ss);
    if (dir.empty()) { err = "animsetluagame: uso: animsetluagame <carpeta>"; return false; }
    FILE* f = fopen((dir + "/juego3dmin.lua").c_str(), "w");
    if (!f) { err = "animsetluagame: no pude escribir " + dir + "/juego3dmin.lua"; return false; }
    fputs("-- juego 3D minimo + ANIMSETS: la franja se prende solo si los clips del armature cargaron\n"
          "function start() end\n"
          "function update(dt)\n"
          "  local fx, fz, rx, rz = cameraXZ()\n"
          "  local w, h = screen()\n"
          "  local v = param(\"loquesea\", 7)\n"
          "  local p = find(\"Personaje\")\n"
          "  local q = find(\"Personaje.002\")\n"
          "  local ok = p and q and animClip(p, \"Correr\") and animLength(p, \"Correr\") == 32\n"
          "            and animLength(q, \"Caminar\") == 31 and animLength(p, \"NoExiste\") == 0\n"
          "  if fx and w and v == 7 and ok then show(object(\"franja\"), true) end\n"
          "end\n", f);
    fclose(f);
    printf("      [animsetluajuego] %s/juego3dmin.lua\n", dir.c_str());
    return true;
}

// saca / devuelve la carpeta animations/ de los assets del juego YA compilado (el staging suelto
// de 'compilegame 0 0'). Va por la shell como juego3dpx: la prueba del juego es de Linux.
bool CmdGameRemove(std::istringstream& ss, std::string& err) {
    const std::string dir = ReadWord(ss);
    std::string mode; ss >> mode;
    if (dir.empty() || (mode != "remove" && mode != "return") || dir.find('\'') != std::string::npos) {
        err = "animsetgameremove: uso: animsetgameremove <carpeta> quitar|volver"; return false;
    }
    const std::string base = dir + "/build/linux/animations";
    struct stat st;
    std::string sh;
    if (mode == "remove") {
        if (stat(base.c_str(), &st) != 0) { err = "animsetgameremove: el juego no tiene " + base; return false; }
        // un '.quitada' de una corrida anterior que se corto no puede frenar el mv
        sh = "rm -rf '" + base + ".quitada' && mv '" + base + "' '" + base + ".quitada'";
    } else {
        if (stat((base + ".quitada").c_str(), &st) != 0) { err = "animsetgameremove: no hay nada que devolver (" + base + ".quitada)"; return false; }
        sh = "rm -rf '" + base + "' && mv '" + base + ".quitada' '" + base + "'";
    }
    if (system(sh.c_str()) != 0) { err = "animsetgameremove: fallo '" + sh + "'"; return false; }
    printf("      [animsetjuegoquitar] %s: %s\n", base.c_str(), mode == "remove" ? "FUERA (el juego no tiene sus .w3da)" : "de vuelta");
    return true;
}

// las pistas de UNA entrada .w3da, contadas por el nombre de su hueso en la tabla
bool CmdTracks(std::istringstream& ss, std::string& err) {
    const std::string path = ReadWord(ss), ent = ReadWord(ss);
    if (path.empty() || ent.empty()) { err = "animsettracks: uso: animsettracks <ruta.w3d> <entrada.w3da> [<hueso> N]..."; return false; }
    W3dZipReader z;
    if (!z.Open(path)) { err = "animsettracks: no puedo abrir " + path; return false; }
    std::vector<unsigned char> d;
    W3dAnimSetData dat;
    std::string reason;
    if (!z.Read(ent, d) || d.empty() || !W3dAnimSetRead(&d[0], d.size(), dat, &reason)) {
        err = "animsettracks: '" + ent + "' no esta o no se lee " + reason; return false;
    }
    std::map<std::string, long> perBone;
    for (size_t c = 0; c < dat.clips.size(); c++) {
        const SkeletalAnimation* cl = dat.clips[c];
        if (!cl) continue;
        for (size_t t = 0; t < cl->tracks.size(); t++) {
            const int b = cl->tracks[t].bone;
            perBone[(b >= 0 && b < (int)dat.bones.size()) ? dat.bones[b] : std::string("-")]++;
        }
    }
    std::string list;
    for (std::map<std::string, long>::const_iterator it = perBone.begin(); it != perBone.end(); ++it) {
        char b[32]; sprintf(b, "=%ld", it->second);
        list += (list.empty() ? "" : " ") + it->first + b;
    }
    printf("      [animsetpistas] %s %s: %d clips, pistas por hueso: %s\n", path.c_str(), ent.c_str(),
           (int)dat.clips.size(), list.c_str());
    std::vector<std::string> k, v;
    if (!ReadAsserts(ss, k, v, "animsettracks", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        std::map<std::string, long>::const_iterator it = perBone.find(k[i]);
        if (!AssertInt("animsettracks", k[i], v[i], it == perBone.end() ? 0 : it->second, err)) return false;
    }
    return true;
}

bool CmdActive(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    Armature* a = ArmByName(n);
    if (!a) { err = "animsetactive: no hay un armature '" + n + "'"; return false; }
    DeselectAll();
    ActiveObj = a;
    printf("      [animsetactivo] '%s'\n", a->name.c_str());
    return true;
}

// renombra un hueso SIN tocar los clips, como el rename de un vertex group que arrastra al hueso
// homonimo del rig (Properties / MeshEdit): el armature sigue colgado de su animset
bool CmdRenameBone(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss), old = ReadWord(ss), newValue = ReadWord(ss);
    Armature* a = ArmByName(n);
    const int i = a ? BoneOf(a, old.c_str()) : -1;
    if (!a || i < 0 || newValue.empty()) { err = "animsetrenamebone: uso: animsetrenamebone <armature> <hueso> <nuevo>"; return false; }
    a->bones[i].name = newValue;
    printf("      [animsetrenombrarhueso] '%s': '%s' -> '%s' (clips %s)\n", a->name.c_str(), old.c_str(), newValue.c_str(),
           W3dArmatureAnimsShared(a) ? "siguen COMPARTIDOS" : "own");
    return true;
}

bool CmdDeleteBone(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss), h = ReadWord(ss);
    Armature* a = ArmByName(n);
    const int i = a ? BoneOf(a, h.c_str()) : -1;
    if (!a || i < 0) { err = "animsetdeletebone: uso: animsetdeletebone <armature> <hueso>"; return false; }
    for (size_t k = 0; k < a->bones.size(); k++) a->bones[k].select = false;
    a->bones[i].select = true; a->activeBone = i;
    const bool ok = BoneEditDelete(a);
    printf("      [animsetborrarhueso] '%s' sin '%s': %s (%d huesos)\n", a->name.c_str(), h.c_str(), ok ? "OK" : "FALLO", (int)a->bones.size());
    if (!ok) { err = "animsetdeletebone: BoneEditBorrar no borro nada"; return false; }
    return true;
}

bool CmdDelete(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : 0;
    if (!o || o == SceneCollection) { err = "animsetdelete: no hay un objeto '" + n + "'"; return false; }
    Object* parent = o->Parent ? o->Parent : SceneCollection;
    for (size_t i = 0; i < parent->Childrens.size(); i++)
        if (parent->Childrens[i] == o) { parent->Childrens.erase(parent->Childrens.begin() + i); break; }
    if (ActiveObj == o) ActiveObj = NULL;
    DeselectAll();
    // ActiveAnimKind / ActiveAnimArm: animation/Animation.h
    if (ActiveAnimArm == (Armature*)o) { ActiveAnimArm = NULL; ActiveAnimKind = 0; }
    W3dFreeSubtree(o);
    printf("      [animsetborrar] '%s' borrado\n", n.c_str());
    return true;
}

// ============================================================================
//  posefoto / posefotoigual
// ============================================================================
void SetBits(std::string& s, const float* f, int n) { s.append((const char*)f, (size_t)n * sizeof(float)); }

std::string HashPose(const Armature* a) {
    std::string s;
    for (size_t i = 0; i < a->bones.size(); i++) {
        const W3dBone& b = a->bones[i];
        const float t[15] = { b.poseT.x, b.poseT.y, b.poseT.z, b.poseR.x, b.poseR.y, b.poseR.z,
                              b.poseS.x, b.poseS.y, b.poseS.z, b.poseHead.x, b.poseHead.y, b.poseHead.z,
                              b.poseTail.x, b.poseTail.y, b.poseTail.z };
        SetBits(s, t, 15);
        SetBits(s, b.poseWorld.m, 16);
        SetBits(s, b.skinMatrix.m, 16);
    }
    unsigned h1, h2;
    W3dAnimSetFootprint((const unsigned char*)s.data(), s.size(), h1, h2);
    char b[40]; sprintf(b, "%08x%08x", h1, h2);
    return b;
}

bool CmdPoseSnapshot(std::istringstream& ss, std::string& err) {
    const std::string name = ReadWord(ss);
    const std::string path = name.empty() ? name : DebugPath(name);
    std::string k; int game = 0;
    // 'sin X': X no entra en la foto; 'solo X': SOLO los nombrados (fotografiar a los demas / al editado)
    std::set<std::string> sin, only;
    while (ss >> k) {
        if (k == "game") ss >> game;
        else if (k == "sin") sin.insert(ReadWord(ss));
        else if (k == "only") only.insert(ReadWord(ss));
        else { err = "posesnapshot: opcion desconocida '" + k + "'"; return false; }
    }
    if (path.empty()) { err = "posesnapshot: uso: posesnapshot <archivo> [juego 0|1] [sin <armature>]... [solo <armature>]..."; return false; }
    std::vector<Armature*> arms;
    CollectArms(SceneCollection, arms);
    for (size_t i = arms.size(); i-- > 0; )
        if (sin.count(arms[i]->name) || (!only.empty() && !only.count(arms[i]->name))) arms.erase(arms.begin() + i);
    // ActiveAnimKind / ActiveAnimArm: animation/Animation.h
    const int kind0 = ActiveAnimKind; Armature* arm0 = ActiveAnimArm;
    std::string out;
    int nPoses = 0, nClips = 0;
    for (size_t ai = 0; ai < arms.size(); ai++) {
        Armature* a = arms[ai];
        const int active0 = a->activeAnim; const float jf0 = a->gameFrame;
        for (size_t ci = 0; ci < a->animations.size(); ci++) {
            SkeletalAnimation* c = a->animations[ci];
            if (!c) continue;
            nClips++;
            a->activeAnim   = (int)ci;
            const int start = c->startFrame, end = c->endFrame, middle = (start + end) / 2;
            const int fr[6] = { start, start + 1, middle, end - 1, end, end + 5 };
            for (int q = 0; q < 6; q++) {
                ActiveAnimKind = 1; ActiveAnimArm = a;
                a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = false;
                EvaluatePoseSkeleton(a, fr[q]);
                char b[64]; sprintf(b, "|%d|f%d|", (int)ci, fr[q]);
                out += a->name + b + c->name + "|" + HashPose(a) + "\n";
                nPoses++;
            }
            if (game) {
                const float length = (float)(end - start);
                const float jf[4]  = { 0.0f, 0.37f, length * 0.5f + 0.5f, length > 0.0f ? length : 0.0f };
                for (int q = 0; q < 4; q++) {
                    ActiveAnimKind = 2; ActiveAnimArm = 0;
                    a->gameFrame = jf[q];
                    a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->lastPoseFrameF = -999999.0f; a->poseDirty = false;
                    EvaluatePoseSkeleton(a, 0);
                    char b[80]; sprintf(b, "|%d|j%.3f|", (int)ci, jf[q]);
                    out += a->name + b + c->name + "|" + HashPose(a) + "\n";
                    nPoses++;
                }
            }
        }
        a->activeAnim = active0; a->gameFrame = jf0;
        a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = false;
    }
    ActiveAnimKind = kind0; ActiveAnimArm = arm0;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { err = "posesnapshot: no puedo escribir " + path; return false; }
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    unsigned h1, h2;
    W3dAnimSetFootprint((const unsigned char*)out.data(), out.size(), h1, h2);
    printf("      [posefoto] %s: %d armatures, %d clips, %d poses, huella %08x%08x\n", path.c_str(), (int)arms.size(), nClips, nPoses, h1, h2);
    return true;
}

bool ReadLines(const std::string& path, std::vector<std::string>& out) {
    out.clear();
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(path, d)) return false;
    std::string cur;
    for (size_t i = 0; i < d.size(); i++) {
        if (d[i] == '\n') { out.push_back(cur); cur.clear(); } else cur += (char)d[i];
    }
    if (!cur.empty()) out.push_back(cur);
    return true;
}

bool CmdPoseSnapshotEqual(std::istringstream& ss, std::string& err) {
    const std::string a = DebugPath(ReadWord(ss)), b = DebugPath(ReadWord(ss));
    std::vector<std::string> la, lb;
    if (!ReadLines(a, la) || !ReadLines(b, lb)) { err = "posesnapshotequal: no puedo leer las fotos"; return false; }
    int distinct = 0;
    const size_t n = la.size() < lb.size() ? la.size() : lb.size();
    for (size_t i = 0; i < n; i++)
        if (la[i] != lb[i]) {
            if (distinct < 5) printf("      [posefotoigual] DISTINTA:\n        %s\n        %s\n", la[i].c_str(), lb[i].c_str());
            distinct++;
        }
    printf("      [posefotoigual] %d contra %d poses, %d distintas\n", (int)la.size(), (int)lb.size(), distinct);
    if (la.empty()) { err = "posesnapshotequal: la foto esta vacia (no hay nada que comparar)"; return false; }
    if (la.size() != lb.size() || distinct) { err = "posesnapshotequal: las fotos NO son identicas"; return false; }
    return true;
}

// ============================================================================
//  animsetromper
// ============================================================================
bool CmdBreak(std::istringstream& ss, std::string& err) {
    const std::string orig = ReadWord(ss), dest = ReadWord(ss);
    std::string mode; ss >> mode;
    if (orig.empty() || dest.empty() || (mode != "cut" && mode != "version" && mode != "req" && mode != "missing")) {
        err = "animsetbreak: uso: animsetbreak <origen.w3d> <destino.w3d> corte|version|req|falta"; return false;
    }
    std::vector<W3dZipEntry> ents;
    if (!W3dZipRead(orig, &ents)) { err = "animsetbreak: no puedo leer " + orig; return false; }
    std::string broken;
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].name == "mimetype") w.Add(ents[i].name, ents[i].data);
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].name == "mimetype") continue;
        const std::string& n = ents[i].name;
        std::vector<unsigned char> d = ents[i].data;
        if (broken.empty() && n.size() > 5 && n.compare(n.size() - 5, 5, ".w3da") == 0 && d.size() > 40) {
            broken = n;
            if (mode == "missing") continue;
            if (mode == "cut") d.resize(d.size() / 2);
            if (mode == "version") d[4] = 9;
            if (mode == "req") { d[40] = 'Z'; d[41] = 'Z'; d[42] = 'Z'; d[43] = 'Z'; }   // el primer fourcc de REQ
        }
        w.Add(n, d);
    }
    if (broken.empty()) { err = "animsetbreak: el contenedor no trae ninguna .w3da"; return false; }
    if (!w.Save(dest)) { err = "animsetbreak: no pude escribir " + dest; return false; }
    printf("      [animsetromper] %s: '%s' %s\n", dest.c_str(), broken.c_str(), mode.c_str());
    return true;
}

// ============================================================================
//  animsetcorrupto: robustez del lector
// ============================================================================
unsigned gRandom = 12345u;
unsigned Random() { gRandom = gRandom * 1103515245u + 12345u; return (gRandom >> 16) & 0x7fffu; }

bool CmdCorrupt(std::string& err) {
    // un animset de verdad: el rig sintetico con sus 4 clips (todos los modos del formato)
    Armature* a = CreateRig(NULL, "RigCorrupto", false, 0.0f);
    const char* names[4] = { "Quieto", "Caminar", "Correr", "Saltar" };
    for (int c = 0; c < 4; c++) a->animations.push_back(SyntheticClip(a, names[c], c));
    std::vector<std::string> bones; W3dArmatureBoneNames(a, bones);
    std::vector<int> map; W3dAnimSetMapFromArmature(bones, a, map);
    std::vector<std::string> blocks(a->animations.size());
    std::vector<const std::string*> ptrs;
    for (size_t c = 0; c < a->animations.size(); c++) {
        W3dAnimSetClipBlock(*a->animations[c], map, (int)bones.size(), blocks[c]);
        ptrs.push_back(&blocks[c]);
    }
    std::string good;
    W3dAnimSetBuild(bones, ptrs, NULL, good);
    // quitarlo de la escena (el constructor lo colgo de SceneCollection y lo selecciono)
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            if (SceneCollection->Childrens[i] == a) { SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + i); break; }
    DeselectAll();
    if (ActiveObj == a) ActiveObj = NULL;
    int faults = 0;
    // 1) lo bueno se lee y se reescribe IGUAL (round-trip de bytes)
    {
        W3dAnimSetData d; std::string m;
        if (!W3dAnimSetRead((const unsigned char*)good.data(), good.size(), d, &m)) { printf("      [animsetcorrupto] el bueno no se lee: %s\n", m.c_str()); faults++; }
        else {
            std::vector<int> id(d.bones.size()); for (size_t i = 0; i < id.size(); i++) id[i] = (int)i;
            std::vector<std::string> b2(d.clips.size()); std::vector<const std::string*> p2;
            for (size_t c = 0; c < d.clips.size(); c++) { W3dAnimSetClipBlock(*d.clips[c], id, (int)d.bones.size(), b2[c]); p2.push_back(&b2[c]); }
            std::string other; W3dAnimSetBuild(d.bones, p2, &d.foreign, other);
            if (other != good) { printf("      [animsetcorrupto] leer + escribir NO da los mismos bytes\n"); faults++; }
            // y los clips son IGUALES campo a campo a los originales (incluidos los bits de los floats)
            for (size_t c = 0; c < d.clips.size() && c < a->animations.size(); c++) {
                const SkeletalAnimation& x = *a->animations[c]; const SkeletalAnimation& y = *d.clips[c];
                bool equal = x.name == y.name && x.FrameRate == y.FrameRate && x.startFrame == y.startFrame &&
                             x.endFrame == y.endFrame && x.tracks.size() == y.tracks.size();
                for (size_t t = 0; equal && t < x.tracks.size(); t++) {
                    const BoneTrack& tx = x.tracks[t]; const BoneTrack& ty = y.tracks[t];
                    equal = tx.bone == ty.bone && tx.Properties.size() == ty.Properties.size();
                    for (size_t p = 0; equal && p < tx.Properties.size(); p++) {
                        const AnimProperty& px = tx.Properties[p]; const AnimProperty& py = ty.Properties[p];
                        equal = px.Property == py.Property && px.component == py.component && px.keyframes.size() == py.keyframes.size();
                        for (size_t q = 0; equal && q < px.keyframes.size(); q++)
                            equal = memcmp(&px.keyframes[q], &py.keyframes[q], sizeof(keyFrame)) == 0;
                    }
                }
                if (!equal) { printf("      [animsetcorrupto] el clip %d no vuelve igual\n", (int)c); faults++; }
            }
        }
    }
    // 2) TODOS los cortes: false, sin clips a medias. La UNICA excepcion es cortar el relleno del
    //    ultimo bloque (la spec lo permite): ahi se acepta y tiene que dar EXACTAMENTE lo mismo.
    int rejects = 0, noFill = 0;
    for (size_t n = 0; n < good.size(); n++) {
        W3dAnimSetData d; std::string m;
        if (W3dAnimSetRead((const unsigned char*)good.data(), n, d, &m)) {
            std::vector<int> id(d.bones.size()); for (size_t i = 0; i < id.size(); i++) id[i] = (int)i;
            std::vector<std::string> b2(d.clips.size()); std::vector<const std::string*> p2;
            for (size_t c = 0; c < d.clips.size(); c++) { W3dAnimSetClipBlock(*d.clips[c], id, (int)d.bones.size(), b2[c]); p2.push_back(&b2[c]); }
            std::string other; W3dAnimSetBuild(d.bones, p2, &d.foreign, other);
            if (n + 4 > good.size() && other == good) noFill++;
            else { printf("      [animsetcorrupto] cortado en %d se ACEPTO\n", (int)n); faults++; }
        }
        else if (!d.clips.empty() || !d.bones.empty()) { printf("      [animsetcorrupto] cortado en %d dejo datos a medias\n", (int)n); faults++; }
        else rejects++;
    }
    // 3) bytes al azar (no se puede caer; si acepta, los indices de hueso tienen que ser validos)
    int accepted = 0, random = 0;
    for (int it = 0; it < 3000; it++) {
        std::string x = good;
        const int nb = 1 + (int)(Random() % 6);
        for (int q = 0; q < nb; q++) x[20 + Random() % (x.size() - 20)] = (char)(Random() & 255);
        W3dAnimSetData d; std::string m;
        if (W3dAnimSetRead((const unsigned char*)x.data(), x.size(), d, &m)) {
            accepted++;
            for (size_t c = 0; c < d.clips.size(); c++)
                for (size_t t = 0; t < d.clips[c]->tracks.size(); t++) {
                    const int b = d.clips[c]->tracks[t].bone;
                    if (b < -1 || b >= (int)d.bones.size()) { printf("      [animsetcorrupto] acepto una pista con hueso %d\n", b); faults++; }
                }
        } else if (!d.clips.empty()) { printf("      [animsetcorrupto] rechazo dejando clips\n"); faults++; }
        random++;
    }
    // 4) version nueva y REQ desconocido: rechazo con motivo
    {
        std::string x = good; x[4] = 2;
        W3dAnimSetData d; std::string m;
        if (W3dAnimSetRead((const unsigned char*)x.data(), x.size(), d, &m) || m.find("version") == std::string::npos) {
            printf("      [animsetcorrupto] version nueva: no se rechazo por version (%s)\n", m.c_str()); faults++; }
        std::string y = good; y[40] = 'Q'; y[41] = 'Q'; y[42] = 'Q'; y[43] = 'Q';
        W3dAnimSetData d2; std::string m2;
        if (W3dAnimSetRead((const unsigned char*)y.data(), y.size(), d2, &m2) || m2.find("QQQQ") == std::string::npos) {
            printf("      [animsetcorrupto] REQ desconocido: no se rechazo por el bloque (%s)\n", m2.c_str()); faults++; }
    }
    // 5) bloque OPCIONAL desconocido: se acepta, se PRESERVA y se reescribe igual
    {
        std::vector<W3dAnimSetForeignBlock> settings(1);
        settings[0].id = "XTRA"; settings[0].domain = 0; settings[0].count = 3; settings[0].data = "abcde";
        std::string withForeign; W3dAnimSetBuild(bones, ptrs, &settings, withForeign);
        W3dAnimSetData d; std::string m;
        if (!W3dAnimSetRead((const unsigned char*)withForeign.data(), withForeign.size(), d, &m) || d.foreign.size() != 1 ||
            d.foreign[0].id != "XTRA" || d.foreign[0].data != "abcde" || d.foreign[0].count != 3) {
            printf("      [animsetcorrupto] el bloque opcional no se preservo (%s)\n", m.c_str()); faults++;
        } else {
            std::vector<std::string> b2(d.clips.size()); std::vector<const std::string*> p2;
            std::vector<int> id(d.bones.size()); for (size_t i = 0; i < id.size(); i++) id[i] = (int)i;
            for (size_t c = 0; c < d.clips.size(); c++) { W3dAnimSetClipBlock(*d.clips[c], id, (int)d.bones.size(), b2[c]); p2.push_back(&b2[c]); }
            std::string other; W3dAnimSetBuild(d.bones, p2, &d.foreign, other);
            if (other != withForeign) { printf("      [animsetcorrupto] con el bloque opcional no se reescribe igual\n"); faults++; }
        }
    }
    delete a;
    printf("      [animsetcorrupto] %d B: %d cortes rechazados (%d solo sin el relleno final: iguales), %d al azar (%d aceptados sin romper nada), %d falla(s)\n",
           (int)good.size(), rejects, noFill, random, accepted, faults);
    if (faults) { err = "animsetcorrupt: el lector .w3da no es robusto (ver arriba)"; return false; }
    return true;
}

// ============================================================================
//  animsetregistro: el codigo, formato/bloques.tsv (filas w3da:) y formato/w3da.md
//  dicen lo mismo. formato/ es gitignoreado: sin la carpeta se saltea ("SIN CORPUS:",
//  como w3dbregistro; W3D_CORPUS_OBLIGATORIO=1 la exige).
// ============================================================================
bool ReadText(const std::string& path, std::string& out) {
    std::vector<unsigned char> b;
    if (!w3dFileSystem::ReadFileBytes(path, b)) return false;
    out.assign(b.empty() ? "" : (const char*)&b[0], b.size());
    return true;
}
bool CmdRegistry(std::string& err) {
    static const char* kBases[] = { "format", "../formato", "../../formato", "../../../formato", "../../../../formato", 0 };
    std::string base;
    for (int i = 0; kBases[i] && base.empty(); i++)
        if (w3dFileSystem::FileExists(std::string(kBases[i]) + "/bloques.tsv")) base = kBases[i];
    if (base.empty()) {
        const char* obl = getenv("W3D_CORPUS_OBLIGATORIO");
        if (obl && *obl && strcmp(obl, "0") != 0) { err = "animsetregistry: no encuentro formato/bloques.tsv"; return false; }
        printf("      [animsetregistro] SIN CORPUS: salteo el cruce con formato/bloques.tsv y formato/w3da.md\n");
        return true;
    }
    std::string tsv, md;
    if (!ReadText(base + "/bloques.tsv", tsv)) { err = "animsetregistry: no pude leer bloques.tsv"; return false; }
    if (!ReadText(base + "/w3da.md", md))      { err = "animsetregistry: no pude leer w3da.md"; return false; }
    std::set<std::string> registered;
    size_t i = 0;
    while (i < tsv.size()) {
        size_t j = tsv.find('\n', i); if (j == std::string::npos) j = tsv.size();
        const std::string ln = tsv.substr(i, j - i);
        i = j + 1;
        if (ln.compare(0, 5, "w3da:") != 0) continue;
        const size_t tb = ln.find('\t');
        std::string f = ln.substr(5, tb == std::string::npos ? std::string::npos : tb - 5);
        for (size_t k = 0; k < f.size(); k++) if (f[k] == '_') f[k] = ' ';
        if (f.size() != 4) { err = "animsetregistry: fila w3da: rota: " + ln; return false; }
        if (ln.find("\tobsoleto\t") == std::string::npos) registered.insert(f);
    }
    std::vector<std::string> known, required;
    W3dAnimSetBlocksKnown(known, &required);
    int bad = 0;
    for (size_t k = 0; k < known.size(); k++) {
        if (!registered.count(known[k])) { printf("      [animsetregistro] '%s' lo entiende el codigo y NO esta en bloques.tsv\n", known[k].c_str()); bad++; }
        if (md.find("`" + known[k] + "`") == std::string::npos) { printf("      [animsetregistro] '%s' no esta documentado en w3da.md\n", known[k].c_str()); bad++; }
    }
    for (std::set<std::string>::const_iterator it = registered.begin(); it != registered.end(); ++it) {
        bool is = false;
        for (size_t k = 0; k < known.size(); k++) if (known[k] == *it) is = true;
        if (!is) { printf("      [animsetregistro] '%s' esta registrado vivo y el codigo no lo entiende\n", it->c_str()); bad++; }
    }
    printf("      [animsetregistro] codigo=%d registrados=%d requeridos=%d -> %s\n", (int)known.size(),
           (int)registered.size(), (int)required.size(), bad ? "MAL" : "OK");
    if (bad) { err = "animsetregistry: el codigo, formato/bloques.tsv y formato/w3da.md no coinciden"; return false; }
    return true;
}

// ============================================================================
//  SELLOS: los clips de un animset cargado son INMUTABLES. Antes de cada comando
//  (y con 'animsetstamps') se re-calcula la huella de CADA clip de cada animset vivo
//  (los canonicos y los de las variantes) y se compara con la que tenia la primera
//  vez que se lo vio: si cambio, algo escribio un clip compartido sin pasar por
//  W3dOwnArmatureAnims (copy-on-write) y le cambio la animacion a TODOS los que
//  lo usan. Es la defensa de la suite contra un camino de edicion nuevo que se
//  olvide de copiar (el editor no tiene red por frame: copia solo el que escribe).
//
//  La clave del sello NO es el puntero del clip (un clip liberado y otro nuevo en la
//  misma direccion darian un falso positivo) sino (animset, huella del archivo del que
//  se leyo, variante, indice del clip): releer el mismo archivo da los mismos clips.
// ============================================================================
struct KeyStamp {
    const void* set; unsigned h1, h2; unsigned long size; int var; unsigned map; int k;
    bool operator<(const KeyStamp& o) const {
        if (set != o.set) return set < o.set;
        if (h1 != o.h1) return h1 < o.h1;
        if (h2 != o.h2) return h2 < o.h2;
        if (size != o.size) return size < o.size;
        if (var != o.var) return var < o.var;
        if (map != o.map) return map < o.map;
        return k < o.k;
    }
};
std::map<KeyStamp, unsigned> gStamps;
bool gStampsOn = true;

// mezcla de 32 bits por PALABRA (no por byte): un nivel grande son ~1,7 M keyframes por verificacion
inline void Mix(unsigned& h, unsigned v) { h ^= v; h *= 16777619u; h ^= h >> 15; }
inline unsigned BitsF(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

unsigned FootprintClip(const SkeletalAnimation* c) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < c->name.size(); i++) Mix(h, (unsigned char)c->name[i]);
    Mix(h, (unsigned)c->FrameRate); Mix(h, (unsigned)c->startFrame); Mix(h, (unsigned)c->endFrame);
    Mix(h, (unsigned)c->tracks.size());
    for (size_t t = 0; t < c->tracks.size(); t++) {
        const BoneTrack& tr = c->tracks[t];
        Mix(h, (unsigned)tr.bone); Mix(h, (unsigned)tr.Properties.size());
        for (size_t p = 0; p < tr.Properties.size(); p++) {
            const AnimProperty& ap = tr.Properties[p];
            Mix(h, (unsigned)ap.Property); Mix(h, (unsigned)ap.component); Mix(h, (unsigned)ap.keyframes.size());
            for (size_t k = 0; k < ap.keyframes.size(); k++) {
                const keyFrame kf = ap.keyframes.Key(k);   // (por valor: verificar no expande una curva compacta)
                Mix(h, (unsigned)kf.frame); Mix(h, BitsF(kf.value));
                Mix(h, (unsigned)kf.Interpolation); Mix(h, (unsigned)kf.handleType);
                Mix(h, BitsF(kf.inDF)); Mix(h, BitsF(kf.inDV)); Mix(h, BitsF(kf.outDF)); Mix(h, BitsF(kf.outDV));
            }
        }
    }
    return h;
}

// false = algun clip compartido cambio ('motivo' dice cual). 'nClips': cuantos se verificaron.
bool StampsVerify(std::string* reason, int* nClips) {
    std::map<KeyStamp, unsigned> seen;
    std::vector<W3dResource*> all;
    W3dResourcesList(W3DREC_ANIMSET, all);
    bool ok = true;
    int n = 0;
    for (size_t i = 0; i < all.size(); i++) {
        const W3dResource* r = all[i];
        if (!r || r->state != W3DREC_READY || !r->data) continue;
        const W3dAnimSet* s = (const W3dAnimSet*)r->data;
        for (int v = -1; v < (int)s->variants.size(); v++) {
            const std::vector<SkeletalAnimation*>& list = (v < 0) ? s->data.clips : s->variants[v]->clips;
            unsigned hm = 2166136261u;
            if (v >= 0) for (size_t m = 0; m < s->variants[v]->map.size(); m++) Mix(hm, (unsigned)s->variants[v]->map[m]);
            for (size_t k = 0; k < list.size(); k++) {
                if (!list[k]) continue;   // vacio: se lo llevo su unico usuario (ya no es del recurso)
                KeyStamp c;
                c.set = s; c.h1 = s->footprint1; c.h2 = s->footprint2; c.size = (unsigned long)s->fileSize;
                c.var = v; c.map = hm; c.k = (int)k;
                const unsigned h = FootprintClip(list[k]);
                std::map<KeyStamp, unsigned>::const_iterator it = gStamps.find(c);
                if (it != gStamps.end() && it->second != h && ok) {
                    ok = false;
                    if (reason) {
                        char b[96]; sprintf(b, "' (clip %d%s) ", (int)k, v >= 0 ? " de una variante" : "");
                        *reason = "el clip COMPARTIDO '" + list[k]->name + b + "del animset '" + s->name +
                                  "' cambio en memoria: algo lo escribio sin copy-on-write (W3dArmatureAnimsPropias)";
                    }
                }
                seen[c] = (it != gStamps.end() && it->second != h) ? it->second : h;
                n++;
            }
        }
    }
    // lo que ya no esta (animsets liberados, casilleros vaciados) se olvida
    gStamps.swap(seen);
    if (nClips) *nClips = n;
    return ok;
}

// el DETECTOR, probado: escribe un keyframe de un clip compartido SIN copiar (lo que haria un camino
// de edicion que se olvide), verifica que los sellos lo vean, y lo deja como estaba
bool StampsTest(const std::string& n, std::string& err) {
    Armature* a = ArmByName(n);
    if (!a || !W3dArmatureAnimsShared(a)) { err = "animsetstamps prueba: '" + n + "' no tiene clips compartidos"; return false; }
    std::string reason;
    if (!StampsVerify(&reason, 0)) { err = "animsetstamps prueba: ya estaban rotos: " + reason; return false; }
    keyFrame* k = 0;
    for (size_t c = 0; c < a->animations.size() && !k; c++) {
        SkeletalAnimation* clip = a->animations[c];
        for (size_t t = 0; clip && t < clip->tracks.size() && !k; t++)
            for (size_t p = 0; p < clip->tracks[t].Properties.size() && !k; p++)
                if (!clip->tracks[t].Properties[p].keyframes.empty()) k = &clip->tracks[t].Properties[p].keyframes[0];
    }
    if (!k) { err = "animsetstamps prueba: sin keyframes"; return false; }
    const float v0 = k->value;
    k->value = v0 + 1.0f;                       // la escritura PROHIBIDA
    const bool seen = !StampsVerify(&reason, 0);
    k->value = v0;                              // como estaba
    const bool sane = StampsVerify(0, 0);
    printf("      [animsetsellos] prueba en '%s': escribir sin copiar %s; restaurado %s\n", n.c_str(),
           seen ? ("SE DETECTA (" + reason + ")").c_str() : "NO SE DETECTA", sane ? "INTACTO" : "ROTO");
    if (!seen) { err = "animsetstamps prueba: una escritura en un clip compartido NO se detecto"; return false; }
    if (!sane)  { err = "animsetstamps prueba: restaurado el valor, los sellos siguen rotos"; return false; }
    return true;
}

// ---- animsetstamps [off|on] [clips N] | animsetsellos prueba <armature> ----
bool CmdStamps(std::istringstream& ss, std::string& err) {
    std::string k;
    long wants = -1;
    while (ss >> k) {
        if (k == "test") return StampsTest(ReadWord(ss), err);
        if (k == "off") { gStampsOn = false; printf("      [animsetsellos] APAGADOS (bench)\n"); return true; }
        if (k == "on")  { gStampsOn = true; }
        else if (k == "clips") { ss >> wants; }
        else { err = "animsetstamps: uso: animsetstamps [off|on] [clips N]"; return false; }
    }
    std::string reason; int n = 0;
    const bool ok = StampsVerify(&reason, &n);
    printf("      [animsetsellos] %d clips compartidos verificados: %s\n", n, ok ? "INTACTOS" : "ALGUNO CAMBIO");
    if (!ok) { err = "animsetsellos: " + reason; return false; }
    if (wants >= 0 && wants != n) { char b[96]; sprintf(b, "animsetstamps: clips = %d, se esperaba %ld", n, wants); err = b; return false; }
    return true;
}

// ============================================================================
//  animsetver / animsetdope: el armature como lo deja el USUARIO para mirar o editar su
//  clip (objeto activo en Pose Mode, todos los huesos elegidos, el clip en el timeline).
//  'animsetshow' hace todo lo que es MIRAR (dope sheet y editor de curvas: filas, dibujo,
//  encuadre, click en un keyframe, la tarjeta Keyframe, reproducir, dibujar la UI entera)
//  y asierta que el armature SIGUE compartiendo sus clips. 'animsetdope' hace UNA edicion
//  por el camino real de la UI y asierta que el armature paso a clips propios; los sellos
//  del proximo comando prueban que el animset (y los demas usuarios) no se entero.
// ============================================================================
void PrepareTimeline(Armature* a, int clip) {
    DeselectAll();
    a->select = true;
    ActiveObj = a;
    state = editNavigation;
    InteractionMode = PoseMode;
    for (size_t i = 0; i < a->bones.size(); i++) a->bones[i].select = true;
    a->activeBone = a->bones.empty() ? -1 : 0;
    ActiveAnimKind = 1; ActiveAnimArm = a;
    a->activeAnim = clip;
    AnimLoadActiveRange();
    a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = true;
}

// y al terminar, de vuelta a Object Mode con la animacion de escena (los comandos siguientes no
// heredan el Pose Mode ni el clip en el timeline)
void ReleaseTimeline() {
    InteractionMode = ObjectMode;
    state = editNavigation;
    ActiveAnimKind = 0; ActiveAnimArm = 0;
    AnimLoadActiveRange();
}

// la primera fila de CANAL con al menos 'minKeys' keyframes (NULL = no hay)
Timeline::DopeRow* RowChannel(Timeline* tl, size_t minKeys) {
    for (size_t i = 0; i < tl->dopeRows.size(); i++)
        if (tl->dopeRows[i].propId >= 0 && tl->dopeRows[i].keys.size() >= minKeys) return &tl->dopeRows[i];
    return 0;
}

// click REAL (CurveClickStrip) sobre el keyframe 'i' de la fila: queda elegido y ACTIVO (tarjeta Keyframe)
bool ClickKeyframe(Timeline* tl, const Timeline::DopeRow& d, size_t i) {
    const AnimProperty* c = tl->RowCurve(d, false);   // solo para ubicar el click
    if (!c || i >= c->keyframes.size()) return false;
    const keyFrame& k = c->keyframes[i];
    tl->viewCenterV = k.value;
    tl->DopeSelectNone();
    return tl->CurveClickStrip((int)(tl->x + tl->FrameToX((float)k.frame)), (int)(tl->y + tl->ValueToY(k.value)));
}

ViewportBase* FindLeaf(ViewportBase* n, int kind) {
    if (!n) return 0;
    if (n->isLeaf()) return n->ViewportKind() == kind ? n : 0;
    ViewportBase* a = FindLeaf(((ViewportColumn*)n)->childA, kind);
    return a ? a : FindLeaf(((ViewportColumn*)n)->childB, kind);
}

bool AssertShared(const char* cmd, Armature* a, std::istringstream& ss, int copies0, std::string& err) {
    std::vector<std::string> k, v;
    if (!ReadAsserts(ss, k, v, cmd, err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        long has = 0;
        if      (k[i] == "shared") has = W3dArmatureAnimsShared(a) ? 1 : 0;
        else if (k[i] == "copies")     has = W3dAnimSetsCopies() - copies0;   // las de ESTE comando
        else { err = std::string(cmd) + ": assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt(cmd, k[i], v[i], has, err)) return false;
    }
    return true;
}

// ---- animsetshow <armature> <clip> [compartido 0|1] [copias N] ----
bool CmdVer(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    int clip = 0; ss >> clip;
    Armature* a = ArmByName(n);
    if (!a || clip < 0 || clip >= (int)a->animations.size()) { err = "animsetshow: uso: animsetshow <armature> <clip> [asserts]"; return false; }
    const int copies0 = W3dAnimSetsCopies();
    PrepareTimeline(a, clip);
    // 1) DOPE SHEET: filas + seleccionar todo + rango de la seleccion
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->BuildDopeRows();
    const int rows = (int)tl->dopeRows.size();
    tl->DopeSelectAll();
    int mn = 0, mx = 0; tl->DopeSelectionRange(mn, mx);
    // 2) EDITOR DE CURVAS: encuadre, costo del trazo, click en un keyframe (hit-test), la tarjeta
    tl->mode = Timeline::TL_MODE_CURVES;
    tl->BuildDopeRows();
    float f0, f1, v0, v1;
    tl->CurveViewRange(false, f0, f1, v0, v1);
    tl->CurveViewRange(true, f0, f1, v0, v1);
    const long long cost = tl->CurveStrokeCost();
    Timeline::DopeRow* channel = RowChannel(tl, 3);
    const bool click = channel && ClickKeyframe(tl, *channel, 1);
    int ki = -1;
    const AnimProperty* kap = DopeActiveKeyframe(&ki, false);
    // la tarjeta "Keyframe" del panel (pestania Animacion): UpdateTabs la refresca
    std::string e2;
    if (!W3dRunCommand("propstab 9", e2)) printf("      [animsetver] (sin panel de propiedades: %s)\n", e2.c_str());
    // 3) la UI ENTERA dibujada una vez, con el timeline del layout en modo curvas (RenderCurves,
    //    el panel del dope, las propiedades, el 3D con el esqueleto posado)
    Timeline* tlLayout = (Timeline*)FindLeaf(rootViewport, 5);
    const int mode0 = tlLayout ? tlLayout->mode : 0;
    if (tlLayout) { tlLayout->mode = Timeline::TL_MODE_CURVES; tlLayout->BuildDopeRows(); }
    LoadPendingTextures();
    if (rootViewport) rootViewport->Render();
    if (tlLayout) tlLayout->mode = mode0;
    // 4) REPRODUCIR: la pose en varios frames del clip
    const SkeletalAnimation* c = a->animations[clip];
    for (int f = c->startFrame; f <= c->endFrame; f += 3) {
        a->lastPoseFrame = -999999; a->poseDirty = false;
        EvaluatePoseSkeleton(a, f);
    }
    tl->DopeSelectNone();
    delete tl;
    ReleaseTimeline();
    printf("      [animsetver] '%s' clip %d: %d filas, keys %d..%d, trazo=%lld vertices, click=%s, tarjeta=%s -> clips %s, copias=%d\n",
           a->name.c_str(), clip, rows, mn, mx, cost, click ? "SI" : "no", kap ? "SI" : "no",
           W3dArmatureAnimsShared(a) ? "COMPARTIDOS" : "PROPIOS", W3dAnimSetsCopies() - copies0);
    if (rows == 0 || !click || !kap) { err = "animsetshow: no se pudo mirar el clip (sin filas / sin keyframe activo)"; return false; }
    return AssertShared("animsetshow", a, ss, copies0, err);
}

// ---- animsetdope <armature> <clip> <op> [compartido 0|1] [copias N] ----
//  op: mover | escalar | cancelar | borrar | duplicar | interp | handletipo | euler | kfvalor |
//      handle | insertar | fin | finigual | renombrar | renombrararm | undo
bool CmdDope(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    int clip = 0; ss >> clip;
    std::string op; ss >> op;
    Armature* a = ArmByName(n);
    if (!a || clip < 0 || clip >= (int)a->animations.size() || op.empty()) { err = "animsetdope: uso: animsetdope <armature> <clip> <op> [asserts]"; return false; }
    const int copies0 = W3dAnimSetsCopies();
    PrepareTimeline(a, clip);
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->mode = Timeline::TL_MODE_CURVES;
    tl->BuildDopeRows();
    Timeline::DopeRow* channel = RowChannel(tl, 3);
    if (!channel) { delete tl; err = "animsetdope: el clip no tiene una curva con 3 keyframes"; return false; }
    const Timeline::DopeRow row = *channel;
    bool done = true;
    if (op == "move" || op == "scale" || op == "cancel") {
        tl->DopeSelectAll();
        tl->DopeMoveStart(op == "scale" ? Timeline::DOPE_ESC : Timeline::DOPE_MOV);
        const char* num = (op == "scale") ? "2" : "3";
        for (const char* q = num; *q; ++q) tl->DopeNumChar(*q);
        if (op == "cancel") tl->DopeMoveCancel(); else tl->DopeMoveConfirm();
    } else if (op == "delete") {
        done = ClickKeyframe(tl, row, 1);
        tl->DopeDeleteSelection();
    } else if (op == "duplicate") {
        done = ClickKeyframe(tl, row, 1);
        tl->DopeDuplicateSelection();
        for (const char* q = "7"; *q; ++q) tl->DopeNumChar(*q);
        tl->DopeMoveConfirm();
    } else if (op == "interp") {
        tl->DopeSelectAll();
        tl->SetInterpolationSel(KfConstant);
    } else if (op == "handletype") {
        tl->DopeSelectAll();
        tl->SetHandleTypeSel(HVector);
    } else if (op == "euler") {
        tl->DopeSelectAll();
        tl->SmartEulerSel();
    } else if (op == "kfvalue") {
        // lo que hacen las acciones de la tarjeta Keyframe (Properties.cpp): la curva EDITABLE
        done  = ClickKeyframe(tl, row, 1);
        int i = -1; AnimProperty* ap = done ? DopeActiveKeyframe(&i) : 0;
        if (ap) { UndoKeyframesStart(); ap->keyframes[i].value += 0.5f; UndoKeyframesConfirm(); }
        done = done && ap;
    } else if (op == "handle") {
        // agarrar el handle de SALIDA de un keyframe BEZIER (el tramo que sale se dibuja curvo, y el
        // handle del keyframe ACTIVO se ve y se agarra) y arrastrarlo: curva el tramo
        done = false;
        for (size_t r = 0; r < tl->dopeRows.size() && !done; r++) {
            const Timeline::DopeRow d = tl->dopeRows[r];
            const AnimProperty* ap = tl->RowCurve(d, false);
            if (!ap) continue;
            size_t k = 0;
            while (k + 1 < ap->keyframes.size() && ap->keyframes[k].Interpolation != KfBezier) k++;
            if (k + 1 >= ap->keyframes.size() || !ClickKeyframe(tl, d, k)) continue;
            // el click elige el keyframe MAS CERCANO (puede ser el de otra curva en el mismo punto): el
            // handle es el del keyframe que quedo ACTIVO
            int ki = -1; const AnimProperty* act = DopeActiveKeyframe(&ki, false);
            if (!act || ki < 0 || act->keyframes[ki].Interpolation != KfBezier) continue;
            float hx = 0, hy = 0;
            tl->HandlePos(act, (size_t)ki, true, hx, hy);
            if (!tl->CurveClickStrip((int)(tl->x + hx), (int)(tl->y + hy)) || !tl->HandleDragging()) continue;
            for (int q = 1; q <= 3; q++) tl->HandleApply((int)(tl->x + hx) + 12 * q, (int)(tl->y + hy) - 15 * q);
            tl->HandleRelease();
            done = true;
        }
    } else if (op == "insert") {
        // Insert Keyframe de Pose Mode (la I) en el SEGUNDO frame del clip (uno de los de posefoto):
        // la pose de ahi con la raiz girada 5 grados
        CurrentFrame = a->animations[clip]->startFrame + 1;
        a->lastPoseFrame = -999999; a->poseDirty = false;
        EvaluatePoseSkeleton(a, CurrentFrame);
        a->bones[0].poseR = a->bones[0].poseR + Vector3(5.0f, 0.0f, 0.0f);
        InsertKeyframeSkeleton(a, 0);
    } else if (op == "rename" || op == "renamearm") {
        // los DOS botones Rename del clip, por el camino real: el boton arranca el rename in-place
        // (toma el PUNTERO al nombre), se tipea y Enter. 'renombrar' = tarjeta Animacion (el clip del
        // timeline); 'renombrararm' = tarjeta del armature activo
        std::string e2;
        W3dRunCommand("propstab 9", e2);   // PropsActive (el panel del layout)
        PropButton* b = !PropsActive ? 0 : (op == "rename") ? PropsActive->propBtnAnimRename : PropsActive->propBtnRenameAnim;
        done = false;
        if (b && b->action) {
            b->action();
            if (g_activeTextField) {
                g_activeTextField->SetText("Renombrado");
                RenameCommit();
                done = (a->animations[clip]->name == "Renombrado");
            }
        }
    } else if (op == "end" || op == "endequal") {
        // el End del clip (campo del timeline / tarjeta Animacion: AnimSetEnd). Copia SOLO si el
        // valor cambia: confirmar el campo sin tocarlo no le cuesta memoria a nadie
        const int end = a->animations[clip]->endFrame;
        AnimSetEnd(op == "end" ? end + 5 : end);
    } else if (op == "undo") {
        UndoUndo();
    } else { delete tl; err = "animsetdope: op desconocida '" + op + "'"; return false; }
    InvalidateAnimAndRedraw();
    tl->DopeSelectNone();
    delete tl;
    ReleaseTimeline();
    printf("      [animsetdope] '%s' clip %d %s: %s -> clips %s, copias=%d\n", a->name.c_str(), clip, op.c_str(),
           done ? "done" : "NO SE PUDO", W3dArmatureAnimsShared(a) ? "COMPARTIDOS" : "PROPIOS",
           W3dAnimSetsCopies() - copies0);
    if (!done) { err = "animsetdope: la operacion '" + op + "' no se pudo hacer"; return false; }
    return AssertShared("animsetdope", a, ss, copies0, err);
}

// ============================================================================
//  LOS ANIMSETS COMO RECURSOS DEL PROYECTO (io/SaveAnimSets.h: lo que usa el
//  outliner por recursos)
//    animsetusuarios <animset> <N>          cuantos armatures lo usan
//    animsetrenombrarset <viejo> <nuevo> [<esperado>]
//    animsetcarpeta <animset> fijar|es <carpeta>
//    animsetpurgar [N]                      saca del registro los huerfanos
// ============================================================================
bool CmdUsers(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    long wants = -1; ss >> wants;
    std::vector<Armature*> us;
    const int k = W3dAnimSetUsers(SceneCollection, n, &us);
    std::string list;
    for (size_t i = 0; i < us.size(); i++) list += (i ? ", " : "") + us[i]->name;
    printf("      [animsetusuarios] '%s': %d (%s)\n", n.c_str(), k, list.c_str());
    if (wants >= 0 && wants != k) { char b[128]; sprintf(b, "animsetusers: %d usuarios, se esperaban %ld", k, wants); err = b; return false; }
    return true;
}
bool CmdRenameSet(std::istringstream& ss, std::string& err) {
    const std::string old = ReadWord(ss), newValue = ReadWord(ss), expected = ReadWord(ss);
    std::string final;
    if (!W3dAnimSetRename(SceneCollection, old, newValue, &final)) { err = "animsetrenameset: no hay un animset '" + old + "'"; return false; }
    printf("      [animsetrenombrarset] '%s' -> '%s'\n", old.c_str(), final.c_str());
    if (!expected.empty() && final != expected) { err = "animsetrenameset: quedo '" + final + "', se esperaba '" + expected + "'"; return false; }
    return true;
}
bool CmdFolder(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    std::string mode; ss >> mode;
    const std::string folder = ReadWord(ss);
    if (mode == "set") {
        if (!W3dAnimSetSetFolder(n, folder)) { err = "animsetfolder: no hay un animset '" + n + "'"; return false; }
        printf("      [animsetcarpeta] '%s' -> carpeta '%s'\n", n.c_str(), folder.c_str());
        return true;
    }
    if (mode != "es") { err = "animsetfolder: uso: animsetfolder <animset> fijar|es <carpeta>"; return false; }
    const std::vector<W3dAnimSetRow>& reg = W3dAnimSetsRegistry();
    for (size_t i = 0; i < reg.size(); i++) {
        if (reg[i].name != n) continue;
        printf("      [animsetcarpeta] '%s' esta en la carpeta '%s'\n", n.c_str(), reg[i].folder.c_str());
        if (reg[i].folder != folder) { err = "animsetfolder: carpeta = '" + reg[i].folder + "', se esperaba '" + folder + "'"; return false; }
        return true;
    }
    err = "animsetfolder: no hay un animset '" + n + "' en el registro";
    return false;
}
bool CmdPurge(std::istringstream& ss, std::string& err) {
    long wants = -1; ss >> wants;
    std::vector<std::string> outside;
    const int k = W3dAnimSetsPurgeOrphans(SceneCollection, &outside);
    std::string list;
    for (size_t i = 0; i < outside.size(); i++) list += (i ? ", " : "") + outside[i];
    printf("      [animsetpurgar] %d huerfanos fuera del registro (%s)\n", k, list.c_str());
    if (wants >= 0 && wants != k) { char b[128]; sprintf(b, "animsetpurge: %d, se esperaban %ld", k, wants); err = b; return false; }
    return true;
}

// ============================================================================
//  CLAVES COMPACTAS (W3dKeys, animation/Animation.h): una curva HORNEADA leida de un .w3da
//  queda en ~4 B por clave. clavessintetica / clavesinfo / clavescomparar / claveseditar
//  (ver la lista de comandos arriba; la prueba es tools/pruebas/prueba_claves_compactas.w3s).
// ============================================================================
long gKeysMark = 0;   // las expansiones al 'keysinfo marca'

// ---- la evaluacion de ANTES de W3dKeys (Animation.cpp), copiada TAL CUAL sobre un std::vector<keyFrame>:
//      la referencia de "sin cambiar ningun resultado" para la curva comun y para la compacta ----
void OldHandle(const std::vector<keyFrame>& kf, size_t i, bool output, float& dF, float& dV) {
    const size_t n = kf.size();
    const keyFrame& k = kf[i];
    dF = 0.0f; dV = 0.0f;
    if (n < 2) return;
    size_t vec = output ? (i + 1 < n ? i + 1 : (i > 0 ? i - 1 : i))
                        : (i > 0 ? i - 1 : (i + 1 < n ? i + 1 : i));
    float length = (float)(kf[vec].frame - k.frame) / 3.0f;
    if (output  && length <= 0.0f) length =  -length;
    if (!output && length >= 0.0f) length =  -length;
    if (length == 0.0f) length = output ? 1.0f : -1.0f;
    if (k.handleType == HFree || k.handleType == HAligned){
        dF = output ? k.outDF : k.inDF;
        dV = output ? k.outDV : k.inDV;
        return;
    }
    if (k.handleType == HVector){
        int df = kf[vec].frame - k.frame;
        if (df == 0){ dF = length; dV = 0.0f; return; }
        float t = length / (float)df;
        dF = length;
        dV = (kf[vec].value - k.value) * t;
        return;
    }
    size_t a = (i == 0) ? 0 : i - 1;
    size_t b = (i + 1 >= n) ? n - 1 : i + 1;
    int spanAB = kf[b].frame - kf[a].frame;
    float m = (spanAB != 0) ? (kf[b].value - kf[a].value) / (float)spanAB : 0.0f;
    if (k.handleType == HAutoClamped && i > 0 && i + 1 < n){
        float dPrev = kf[i-1].value - k.value;
        float dNext = kf[i+1].value - k.value;
        if ((dPrev > 0.0f && dNext > 0.0f) || (dPrev < 0.0f && dNext < 0.0f)) m = 0.0f;
    }
    dF = length;
    dV = m * length;
    if (k.handleType == HAutoClamped){
        float vv = kf[vec].value;
        float lo = (k.value < vv) ? k.value : vv;
        float hi = (k.value > vv) ? k.value : vv;
        float cp = k.value + dV;
        if (cp < lo) dV = lo - k.value;
        else if (cp > hi) dV = hi - k.value;
    }
}
float OldBezier(const std::vector<keyFrame>& kf, size_t i, float frame) {
    const keyFrame& a = kf[i-1];
    const keyFrame& b = kf[i];
    const float x0 = (float)a.frame, x3 = (float)b.frame;
    const float span = x3 - x0;
    if (span <= 0.0f) return b.value;
    // (2026-11: en las puntas, el valor exacto del keyframe; ver AnimProperty::EvalBezier)
    if (frame >= x3) return b.value;
    if (frame <= x0) return a.value;
    float aDF, aDV, bDF, bDV;
    OldHandle(kf, i-1, true,  aDF, aDV);
    OldHandle(kf, i,   false, bDF, bDV);
    float x1 = x0 + aDF, x2 = x3 + bDF;
    if (x1 < x0) x1 = x0; if (x1 > x3) x1 = x3;
    if (x2 < x0) x2 = x0; if (x2 > x3) x2 = x3;
    const float y0 = a.value, y1 = a.value + aDV, y2 = b.value + bDV, y3 = b.value;
    float lo = 0.0f, hi = 1.0f, t = 0.5f;
    for (int it = 0; it < 24; it++){
        t = (lo + hi) * 0.5f;
        float u = 1.0f - t;
        float x = u*u*u*x0 + 3.0f*u*u*t*x1 + 3.0f*u*t*t*x2 + t*t*t*x3;
        if (x < frame) lo = t; else hi = t;
    }
    // (2026-10: con los handles a un tercio del tramo en x -automaticos y vector- x(t) es una recta y EvalBezier
    //  despeja t directo, sin biseccion. La referencia hace lo mismo; la biseccion de arriba queda de control: las
    //  dos tienen que dar casi lo mismo, si no el atajo esta mal)
    if (aDF == span / 3.0f && bDF == -span / 3.0f) {
        float td = (frame - x0) / span;
        if (td < 0.0f) td = 0.0f; else if (td > 1.0f) td = 1.0f;
        if (fabsf(td - t) > 1e-5f) { float nan; unsigned un = 0x7FC00001u; memcpy(&nan, &un, 4); return nan; }
        t = td;
    }
    float u = 1.0f - t;
    return u*u*u*y0 + 3.0f*u*u*t*y1 + 3.0f*u*t*t*y2 + t*t*t*y3;
}
float OldEvalF(const std::vector<keyFrame>& kf, float frame, float def) {
    const size_t n = kf.size();
    if (n == 0) return def;
    if (frame <= (float)kf[0].frame)     return kf[0].value;
    if (frame >= (float)kf[n-1].frame)   return kf[n-1].value;
    for (size_t i = 1; i < n; i++) {
        if ((float)kf[i].frame < frame) continue;
        const keyFrame& a = kf[i-1];
        const keyFrame& b = kf[i];
        if (a.Interpolation == KfConstant) return (frame >= (float)b.frame) ? b.value : a.value;
        int span = b.frame - a.frame;
        if (span <= 0) return b.value;
        float t = (frame - (float)a.frame) / (float)span;
        if (a.Interpolation != KfBezier)
            return a.value + (b.value - a.value) * t;
        return OldBezier(kf, i, frame);
    }
    return kf[n-1].value;
}

// una curva HORNEADA: n claves desde 'primero' cada 'paso', todas con la misma interpolacion y tipo de handle,
// handles en cero (lo que trae un glTF importado). Valores deterministas.
AnimProperty CurveBaked(int prop, int comp, int first, int step, int n, int interp, int typeH,
                           float base, float amp, float phase) {
    AnimProperty ap; ap.Property = prop; ap.component = comp;
    for (int i = 0; i < n; i++) {
        keyFrame k; k.frame = first + step * i; k.Interpolation = interp; k.handleType = typeH;
        k.value = base + (float)(amp * sin(0.23 * i + phase));
        ap.keyframes.push_back(k);
    }
    return ap;
}
// un clip con curvas que se COMPACTAN al leerlas (todas las interpolaciones y tipos de handle, paso 1 / 2 / 3 / 5,
// un primer frame negativo, frames enormes -fuera de lo exacto en float-, un -0.0 y un tramo plano) y otras que
// NO (un solo key, frames irregulares, un handle guardado en -0.0): 22 compactas + 3 comunes
SkeletalAnimation* ClipBake(const Armature* a, const std::string& name) {
    SkeletalAnimation* c = new SkeletalAnimation(name);
    c->FrameRate = 30; c->startFrame = 1; c->endFrame = 60;
    for (int h = 0; h < 5; h++) {
        BoneTrack tr;
        tr.bone = BoneOf(a, kBones[h]);
        tr.Properties.push_back(CurveBaked(AnimRotation, AnimX, 1, 1, 60, KfBezier, HAuto, 0.0f, 25.0f, 0.5f * h));
        tr.Properties.push_back(CurveBaked(AnimRotation, AnimY, 1, 1, 60, KfBezier, HAutoClamped, 0.0f, 18.0f, 0.9f * h));
        tr.Properties.push_back(CurveBaked(AnimRotation, AnimZ, 1, 1, 60, KfBezier, HVector, 0.0f, 12.0f, 1.3f * h));
        if (h == 0) {
            tr.Properties.push_back(CurveBaked(AnimPosition, AnimX, 1, 1, 60, KfLinear, HAuto, 0.0f, 0.8f, 0.1f));
            tr.Properties.push_back(CurveBaked(AnimPosition, AnimY, 1, 3, 20, KfConstant, HAuto, 0.5f, 0.3f, 0.2f));
            tr.Properties.push_back(CurveBaked(AnimPosition, AnimZ, 1, 1, 60, KfBezier, HFree, 0.0f, 0.6f, 0.3f));
        }
        if (h == 1) {
            tr.Properties.push_back(CurveBaked(AnimScale, AnimX, -7, 2, 40, KfBezier, HAuto, 1.0f, 0.2f, 0.4f));
            AnimProperty one; one.Property = AnimScale; one.component = AnimY;   // UN key: comun
            keyFrame k; k.frame = 9; k.value = 1.25f; one.keyframes.push_back(k);
            tr.Properties.push_back(one);
            AnimProperty irr = CurveBaked(AnimScale, AnimZ, 1, 1, 6, KfBezier, HAuto, 1.0f, 0.1f, 0.0f);
            irr.keyframes[3].frame = 5; irr.keyframes[4].frame = 9; irr.keyframes[5].frame = 30;   // irregulares: comun
            tr.Properties.push_back(irr);
        }
        if (h == 2) {
            tr.Properties.push_back(CurveBaked(AnimPosition, AnimX, 16777200, 3, 24, KfLinear, HAuto, 0.0f, 0.5f, 0.6f));
            tr.Properties.push_back(CurveBaked(AnimPosition, AnimY, 1, 5, 13, KfBezier, HAutoClamped, 0.2f, 0.4f, 0.7f));
            AnimProperty han = CurveBaked(AnimPosition, AnimZ, 1, 1, 60, KfBezier, HAuto, 0.0f, 0.3f, 0.8f);
            han.keyframes[7].inDV = -0.0f;   // un handle guardado (bits no cero): comun, como en el archivo
            tr.Properties.push_back(han);
        }
        if (h == 3) {
            AnimProperty rare = CurveBaked(AnimPosition, AnimX, 1, 1, 60, KfBezier, HAutoClamped, 0.0f, 0.5f, 0.0f);
            rare.keyframes[0].value = -0.0f;
            for (int q = 20; q < 30; q++) rare.keyframes[q].value = 0.75f;   // un tramo plano (los picos del clamped)
            tr.Properties.push_back(rare);
        }
        c->tracks.push_back(tr);
    }
    return c;
}
// un clip CORTO: curvas de 2 claves (las de un keyframe al principio y otro al final) y un escalon de 3
SkeletalAnimation* ShortClipBake(const Armature* a, const std::string& name) {
    SkeletalAnimation* c = new SkeletalAnimation(name);
    c->FrameRate = 24; c->startFrame = 1; c->endFrame = 11;
    for (int h = 0; h < 5; h++) {
        BoneTrack tr;
        tr.bone = BoneOf(a, kBones[h]);
        tr.Properties.push_back(CurveBaked(AnimRotation, AnimX, 1, 10, 2, KfBezier, HAuto, 0.0f, 40.0f, 0.3f * h));
        tr.Properties.push_back(CurveBaked(AnimRotation, AnimY, 1, 10, 2, KfLinear, HAuto, 0.0f, 30.0f, 0.5f * h));
        tr.Properties.push_back(CurveBaked(AnimRotation, AnimZ, 1, 10, 2, KfBezier, HVector, 0.0f, 20.0f, 0.7f * h));
        if (h == 0) tr.Properties.push_back(CurveBaked(AnimPosition, AnimY, 1, 5, 3, KfConstant, HAuto, 0.0f, 1.0f, 0.0f));
        c->tracks.push_back(tr);
    }
    return c;
}

// ---- keyssynthetic [ruta.w3d] ----
bool CmdKeysSynthetic(std::istringstream& ss, std::string& err) {
    std::string path = ReadWord(ss), extra;
    bool inverted = false;
    if (path == "inverted") { inverted = true; path.clear(); }
    while (ss >> extra) if (extra == "inverted") inverted = true;
    RestartScene();
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Personajes");
    CollectionActive = col;
    // dos armatures con los MISMOS clips (el guardado los deja en UN animset) y, con 'invertido', uno con los
    // huesos en OTRO orden: al abrir usa los mismos clips por una VARIANTE (una copia reindexada)
    const int total = inverted ? 3 : 2;
    for (int i = 0; i < total; i++) {
        Armature* a = CreateRig(col, i < 2 ? "Horneado" : "Invertido", i >= 2, 2.0f * (float)i);
        a->animations.push_back(ClipBake(a, "Largo"));
        a->animations.push_back(ShortClipBake(a, "Corto"));
        a->activeAnim = 0;
    }
    DeselectAll();
    ActiveObj = NULL;
    printf("      [clavessintetica] %d armatures con los clips horneados 'Largo' y 'Corto'\n", total);
    if (!path.empty()) {
        const bool ok = SaveW3D(path);
        printf("      [clavessintetica] guardada en %s -> %s\n", path.c_str(), ok ? "OK" : "FALLO");
        if (!ok) { err = "keyssynthetic: no pude guardar " + path; return false; }
    }
    return true;
}

// ---- keysinfo [marca] [compactas N] [nuevas N] [maxbpc X] ----
bool CmdKeysInfo(std::istringstream& ss, std::string& err) {
    W3dKeysStats st;
    W3dKeysGetStats(st);
    const double bpc = st.compactKeys ? (double)st.compactBytes / (double)st.compactKeys : 0.0;
    printf("      [clavesinfo] compactas=%ld curvas (%ld claves, %ld B = %.2f B/clave) bytes vivos=%ld expandidas=%ld (%ld desde la marca)\n",
           st.compact, st.compactKeys, st.compactBytes, bpc, st.bytes, st.expansions, st.expansions - gKeysMark);
    std::string k;
    while (ss >> k) {
        if (k == "mark") { gKeysMark = st.expansions; continue; }
        if (k == "maxbpc") {
            double v = 0.0; ss >> v;
            if (bpc > v) { char b[128]; sprintf(b, "keysinfo: %.2f B por clave compacta, se esperaban <= %.2f", bpc, v); err = b; return false; }
            continue;
        }
        long v = -1; ss >> v;
        long has = 0;
        if      (k == "compact") has = st.compact;
        else if (k == "new")    has = st.expansions - gKeysMark;
        else { err = "keysinfo: assert desconocido '" + k + "'"; return false; }
        if (has != v) { char b[160]; sprintf(b, "keysinfo %s: hay %ld, se esperaban %ld", k.c_str(), has, v); err = b; return false; }
    }
    return true;
}

// los bits de un float (NaN incluido: la misma cuenta da el mismo NaN)
bool SameBitsF(float a, float b) { return memcmp(&a, &b, sizeof(float)) == 0; }

// ---- keyscompare <armature> [paso X] [compactas N] ----
bool CmdKeysCompare(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    float step = 0.125f; long wants = -1;
    std::string k;
    while (ss >> k) {
        if (k == "step") ss >> step;
        else if (k == "compact") ss >> wants;
        else { err = "keyscompare: opcion desconocida '" + k + "'"; return false; }
    }
    // las listas de curvas: las pistas de los clips de un ARMATURE o las de los clips de jerarquia de la
    // biblioteca de una RAIZ (bloques JCLP)
    std::vector<const std::vector<AnimProperty>*> lists;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : 0;
    if (o && o->getType() == ObjectType::armature) {
        const Armature* a = (const Armature*)o;
        for (size_t ci = 0; ci < a->animations.size(); ci++)
            for (size_t t = 0; a->animations[ci] && t < a->animations[ci]->tracks.size(); t++)
                lists.push_back(&a->animations[ci]->tracks[t].Properties);
    } else if (o && W3dHierAnimSet(o)) {
        const W3dAnimSet* s = W3dHierAnimSet(o);
        for (size_t j = 0; j < s->data.hierarchies.size(); j++)
            for (size_t t = 0; s->data.hierarchies[j] && t < s->data.hierarchies[j]->tracks.size(); t++)
                lists.push_back(&s->data.hierarchies[j]->tracks[t].props);
    }
    if (!o || lists.empty() || !(step > 0.0f && step < 1.0f)) {
        err = "keyscompare: uso: keyscompare <armature|raiz de clips de jerarquia> [paso X] [compactas N]"; return false;
    }
    W3dKeysStats st0;
    W3dKeysGetStats(st0);
    long curves = 0, compact = 0, evals = 0, handles = 0, bad = 0;
    std::string first;
    unsigned seed = 12345u;
    for (size_t l = 0; l < lists.size(); l++) {
            for (size_t p = 0; p < lists[l]->size(); p++) {
                const AnimProperty& ap = (*lists[l])[p];
                curves++;
                if (ap.keyframes.Compact()) compact++;
                // las MISMAS claves en un vector (por valor: la curva no se toca) y una curva COMUN con ellas
                std::vector<keyFrame> v;
                for (size_t i = 0; i < ap.keyframes.size(); i++) v.push_back(ap.keyframes.Key(i));
                AnimProperty common; common.Property = ap.Property; common.component = ap.component;
                common.keyframes = v;
                char where[96]; sprintf(where, "pista %d curva %d", (int)l, (int)p);
                if (!ap.keyframes.SameBits(common.keyframes) || !common.keyframes.SameBits(ap.keyframes)) {
                    if (!bad++) first = std::string(where) + ": las claves no son las mismas";
                }
                if (v.empty()) continue;
                // los frames: todos los enteros del rango (+-3), fracciones de 'paso', algunas al azar y los raros
                std::vector<float> fs;
                const int f0 = ap.keyframes.FirstFrame() - 3, f1 = ap.keyframes.LastFrame() + 3;
                for (int f = f0; f <= f1; f++) {
                    fs.push_back((float)f);
                    for (float d = step; d < 1.0f; d += step) fs.push_back((float)f + d);
                    seed = seed * 1103515245u + 12345u;
                    fs.push_back((float)f + (float)((seed >> 8) & 0xFFFF) / 65536.0f);
                }
                const float rare[4] = { 0.0f, -1.0e30f, 1.0e30f, (float)(f0 + f1) * 0.5f };
                for (int q = 0; q < 4; q++) fs.push_back(rare[q]);
                float nan; unsigned un = 0x7FC00000u; memcpy(&nan, &un, 4); fs.push_back(nan);
                for (size_t q = 0; q < fs.size(); q++) {
                    const float r = OldEvalF(v, fs[q], 7.0f);
                    const float x = ap.EvalF(fs[q], 7.0f), y = common.EvalF(fs[q], 7.0f);
                    evals++;
                    if (!SameBitsF(r, x) || !SameBitsF(r, y)) {
                        if (!bad++) {
                            char b[200]; sprintf(b, ": EvalF(%.6f) antes=%.9g compacta=%.9g comun=%.9g", fs[q], r, x, y);
                            first = std::string(where) + b;
                        }
                    }
                }
                for (int f = f0; f <= f1; f++) {   // Eval(int): el editor y el reposo
                    const float r = OldEvalF(v, (float)f, -3.0f);
                    if (!SameBitsF(r, ap.Eval(f, -3.0f)) || !SameBitsF(r, common.Eval(f, -3.0f)))
                        if (!bad++) first = std::string(where) + ": Eval(int) distinto";
                }
                // los handles efectivos de cada clave, de los dos lados (el editor de curvas los dibuja)
                for (size_t i = 0; i < v.size(); i++)
                    for (int s = 0; s < 2; s++) {
                        float rf, rv, xf, xv, yf, yv;
                        OldHandle(v, i, s != 0, rf, rv);
                        ap.EffectiveHandle(i, s != 0, xf, xv);
                        common.EffectiveHandle(i, s != 0, yf, yv);
                        handles++;
                        if (!SameBitsF(rf, xf) || !SameBitsF(rv, xv) || !SameBitsF(rf, yf) || !SameBitsF(rv, yv))
                            if (!bad++) first = std::string(where) + ": HandleEfectivo distinto";
                    }
            }
    }
    W3dKeysStats st1;
    W3dKeysGetStats(st1);
    printf("      [clavescomparar] '%s': %ld curvas (%ld compactas), %ld evaluaciones y %ld handles contra la evaluacion de "
           "antes: %ld distintas; expandidas al comparar=%ld\n", n.c_str(), curves, compact, evals, handles, bad,
           st1.expansions - st0.expansions);
    if (bad) { err = "clavescomparar: " + first; return false; }
    if (st1.expansions != st0.expansions) { err = "keyscompare: mirar las curvas EXPANDIO alguna compacta"; return false; }
    if (wants >= 0 && wants != compact) {
        char b[128]; sprintf(b, "keyscompare: %ld curvas compactas, se esperaban %ld", compact, wants); err = b; return false;
    }
    return true;
}

// cuantas curvas del clip son compactas
long CompactOf(const SkeletalAnimation* c) {
    long k = 0;
    for (size_t t = 0; c && t < c->tracks.size(); t++)
        for (size_t p = 0; p < c->tracks[t].Properties.size(); p++) if (c->tracks[t].Properties[p].keyframes.Compact()) k++;
    return k;
}

// ---- keysedit <armature> <clip> insertar|mover|borrar ----
bool CmdKeysEdit(std::istringstream& ss, std::string& err) {
    const std::string n = ReadWord(ss);
    int clip = -1; ss >> clip;
    std::string op; ss >> op;
    Armature* a = ArmByName(n);
    if (!a || clip < 0 || clip >= (int)a->animations.size() || (op != "insert" && op != "move" && op != "delete")) {
        err = "keysedit: uso: keysedit <armature> <clip> insertar|mover|borrar"; return false;
    }
    PrepareTimeline(a, clip);     // el clip en el timeline: el undo de keyframes captura ESTE
    W3dOwnArmatureAnims(a);       // copy-on-write, como el editor antes de escribir (copia las compactas compactas)
    SkeletalAnimation* c = a->animations[clip];
    // la primera curva compacta del clip con 4 claves o mas
    size_t t = 0, p = 0; bool has = false;
    for (size_t tt = 0; c && tt < c->tracks.size() && !has; tt++)
        for (size_t pp = 0; pp < c->tracks[tt].Properties.size() && !has; pp++) {
            const W3dKeys& k = c->tracks[tt].Properties[pp].keyframes;
            if (k.Compact() && k.size() >= 4) { t = tt; p = pp; has = true; }
        }
    if (!has) { ReleaseTimeline(); err = "keysedit: el clip no tiene una curva compacta de 4 claves o mas"; return false; }
    const AnimProperty prev = c->tracks[t].Properties[p];   // (copia COMPACTA)
    const long other = CompactOf(c) - 1;
    const size_t n0 = prev.keyframes.size();
    W3dKeysStats s0;
    W3dKeysGetStats(s0);
    UndoKeyframesStart();
    {
        AnimProperty& ap = c->tracks[t].Properties[p];
        if (op == "move")         ap.keyframes[3].value += 1.5f;                        // el editor de curvas
        else if (op == "insert") SetKeyCurve(ap, ap.keyframes.LastFrame() + 5, 2.5f); // la I
        else                       DeleteKeyframeKeepingShape(ap, ap.keyframes.Frame(2)); // la X del dope sheet
    }
    UndoKeyframesConfirm();
    W3dKeysStats s1;
    W3dKeysGetStats(s1);
    const AnimProperty& ap = c->tracks[t].Properties[p];
    const long newValue = s1.expansions - s0.expansions, otherAfter = CompactOf(c);
    // la edicion se ve: el valor movido / la clave nueva / la que falta
    bool seVe = false;
    if (op == "move")         seVe = ap.keyframes.size() == n0 && SameBitsF(ap.keyframes.Value(3), prev.keyframes.Value(3) + 1.5f);
    else if (op == "insert") seVe = ap.keyframes.size() == n0 + 1 && ap.Eval(prev.keyframes.LastFrame() + 5, 0.0f) == 2.5f;
    else                       seVe = ap.keyframes.size() == n0 - 1 && ap.keyframes.Frame(2) == prev.keyframes.Frame(3);
    const bool common = !ap.keyframes.Compact();
    // Ctrl+Z: las MISMAS claves de antes, bit a bit, y compacta otra vez (el paso de undo guardo la compacta)
    UndoUndo();
    const AnimProperty& ap2 = c->tracks[t].Properties[p];
    const bool undone = ap2.keyframes.SameBits(prev.keyframes) && ap2.keyframes.Compact();
    // Ctrl+Y: la edicion vuelve
    UndoRedo();
    const AnimProperty& ap3 = c->tracks[t].Properties[p];
    const bool redone = !ap3.keyframes.Compact() && !ap3.keyframes.SameBits(prev.keyframes) &&
                         ap3.keyframes.size() == (op == "insert" ? n0 + 1 : op == "delete" ? n0 - 1 : n0);
    ReleaseTimeline();
    printf("      [claveseditar] '%s' clip %d %s (pista %d curva %d, %d claves): expandidas=%ld, la curva %s, las otras %ld de %ld "
           "compactas, se ve %s, Ctrl+Z %s, Ctrl+Y %s\n", a->name.c_str(), clip, op.c_str(), (int)t, (int)p, (int)n0, newValue,
           common ? "COMUN" : "compact", otherAfter, other, seVe ? "SI" : "NO", undone ? "OK (compacta)" : "MAL",
           redone ? "OK" : "MAL");
    if (newValue != 1) { char b[96]; sprintf(b, "keysedit: se expandieron %ld curvas (tiene que ser SOLO la editada)", newValue); err = b; return false; }
    if (!common || otherAfter != other) { err = "keysedit: la curva editada no quedo comun o se expandio otra del clip"; return false; }
    if (!seVe) { err = "keysedit: la edicion no se ve en la curva"; return false; }
    if (!undone) { err = "keysedit: Ctrl+Z no devolvio las mismas claves (compactas)"; return false; }
    if (!redone) { err = "keysedit: Ctrl+Y no rehizo la edicion"; return false; }
    return true;
}

} // namespace

// la linea de los animsets de 'meminfo' (W3dScript.cpp la llama; el resto del informe es de alla)
void W3dTestsAnimsMeminfo() {
    W3dAnimSetsStats st;
    W3dAnimSetsGetStats(st);
    // lo que ocuparian los clips de los armatures COLGADOS si cada uno tuviera los suyos
    std::vector<Armature*> arms;
    CollectArms(SceneCollection, arms);
    long sin = 0;
    for (size_t i = 0; i < arms.size(); i++) {
        if (!W3dArmatureAnimsShared(arms[i])) continue;
        for (size_t c = 0; c < arms[i]->animations.size(); c++) sin += W3dAnimSetClipBytes(arms[i]->animations[c], 0);
    }
    const long saving = sin > st.bytes ? sin - st.bytes : 0;
    printf("      [meminfo] animsets: vivos=%d refs=%d clips=%d (+%d de variantes) keys=%ld compartido=%ld B (%.1f KB) AHORRO=%ld B (%.1f KB) copias COW=%d\n",
           st.live, st.refs, st.clips, st.clipsVariant, st.keys, st.bytes, st.bytes / 1024.0, saving, saving / 1024.0,
           W3dAnimSetsCopies());
    // las claves de TODAS las curvas vivas (W3dKeys, Animation.h): las compactas y las expandidas desde el arranque
    W3dKeysStats cs;
    W3dKeysGetStats(cs);
    printf("      [meminfo] claves: bytes vivos=%ld B (%.1f KB) compactas=%ld curvas (%ld claves, %ld B = %.2f B/clave) expandidas=%ld (desde el arranque)\n",
           cs.bytes, cs.bytes / 1024.0, cs.compact, cs.compactKeys, cs.compactBytes,
           cs.compactKeys ? (double)cs.compactBytes / (double)cs.compactKeys : 0.0, cs.expansions);
}

bool W3dTestsAnimsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    // SELLOS: ningun clip de un animset cargado puede haber cambiado desde el comando anterior
    // (el editor copia solo al escribir: un camino que escriba sin copiar se ve aca, no en la
    // animacion de otro personaje). 'animsetstamps off' lo apaga para medir.
    if (gStampsOn && cmd != "animsetstamps") {
        std::string reason;
        if (!StampsVerify(&reason, 0)) { err = "animsets (antes de este comando): " + reason; return false; }
    }
    // los comandos de W3dScript.cpp que escriben el clip del armature ACTIVO sin pasar por el Core
    // (atajos del harness, no caminos del editor): copian antes, como lo haria el editor. Los demas
    // 'anim' (add/dup/del/up/down) van por el Core / el undo, que copian solos (o no escriben clips)
    if ((cmd == "anim" || cmd == "animkey") && ActiveObj && ActiveObj->getType() == ObjectType::armature) {
        std::istringstream s2(ss.str());
        std::string c0, sub;
        s2 >> c0 >> sub;
        if (cmd == "animkey" || sub == "rename") W3dOwnArmatureAnims((Armature*)ActiveObj);
    }
    if (cmd == "animsetinfo")      return CmdInfo(ss, err);
    if (cmd == "animsetarm")       return CmdArm(ss, err);
    if (cmd == "animsetrec")       return CmdRec(ss, err);
    if (cmd == "animsetclip")      return CmdClip(ss, err);
    if (cmd == "animsetsw3d")      return CmdSw3d(ss, err);
    if (cmd == "animnoline")      { int v = 0; ss >> v; g_w3dAnimsInline = (v != 0);
                                     printf("      [animsinline] %s\n", g_w3dAnimsInline ? "los clips se guardan INLINE (formato de antes)" : "animsets .w3da");
                                     return true; }
    if (cmd == "animsetsynthetic") return CmdSynthetic(ss, err);
    if (cmd == "animsetpoint")   return CmdPoint(ss, err);
    if (cmd == "animsetrename") return CmdRename(ss, err);
    if (cmd == "animsetdelete")    return CmdDelete(ss, err);
    if (cmd == "animsetactive")    return CmdActive(ss, err);
    if (cmd == "animsetluagame")  return CmdLuaGame(ss, err);
    if (cmd == "animsetgameremove") return CmdGameRemove(ss, err);
    if (cmd == "animsettracks")    return CmdTracks(ss, err);
    if (cmd == "animsetdeletebone") return CmdDeleteBone(ss, err);
    if (cmd == "animsetrenamebone") return CmdRenameBone(ss, err);
    if (cmd == "posesnapshot")         return CmdPoseSnapshot(ss, err);
    if (cmd == "posesnapshotequal")    return CmdPoseSnapshotEqual(ss, err);
    if (cmd == "animsetbreak")    return CmdBreak(ss, err);
    if (cmd == "animsetcorrupt")  return CmdCorrupt(err);
    if (cmd == "animsetregistry")  return CmdRegistry(err);
    if (cmd == "animsetstamps")    return CmdStamps(ss, err);
    if (cmd == "animsetshow")       return CmdVer(ss, err);
    if (cmd == "animsetdope")      return CmdDope(ss, err);
    if (cmd == "animsetusers")  return CmdUsers(ss, err);
    if (cmd == "animsetrenameset") return CmdRenameSet(ss, err);
    if (cmd == "animsetfolder")   return CmdFolder(ss, err);
    if (cmd == "animsetpurge")    return CmdPurge(ss, err);
    if (cmd == "keyssynthetic")  return CmdKeysSynthetic(ss, err);
    if (cmd == "keysinfo")       return CmdKeysInfo(ss, err);
    if (cmd == "keyscompare")   return CmdKeysCompare(ss, err);
    if (cmd == "keysedit")     return CmdKeysEdit(ss, err);
    handled = false;
    return false;
}
