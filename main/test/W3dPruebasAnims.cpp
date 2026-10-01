// ============================================================================
//  W3dPruebasAnims.cpp — comandos de harness de los ANIMSETS (ver W3dPruebasAnims.h
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
//        'nombreset' = su animSetNombre (la biblioteca a la que lo manda el guardado).
//    animsetclip <armature> <i> [nombre X] [keys N] [mismo <armature2> <j> 0|1]
//        el clip i de un armature; 'mismo' asserta si es EL MISMO objeto en memoria que el
//        clip j de otro (compartido de verdad) o no.
//    animsetsw3d <ruta.w3d> [animsets N] [clips N] [inline N] [armatures N] [jsonmaxkb N] [bytesmaxkb N]
//        lo que hay ADENTRO de un contenedor: las entradas .w3da (cada una tiene que leerse
//        entera), sus clips, el registro, los armatures que nombran animset y los que traen
//        los clips INLINE (el formato de antes), y el tamano de proyecto.json.
//    animsinline 0|1
//        el guardado escribe los clips INLINE como antes (para fabricar archivos "viejos").
//    animsetsintetica <n> [ruta.w3d] [invertido] [agregar]
//        escena NUEVA con n armatures del MISMO rig (5 huesos), cada uno con 4 clips; el
//        segundo con un "Quieto" distinto y el tercero con un clip repetido. Los clips cubren
//        todos los modos del formato (horneados, bezier con handles, frames irregulares,
//        interpolaciones mezcladas, -0.0, una pista sin hueso y una repetida). 'invertido'
//        agrega uno mas con los huesos en OTRO orden (mismos nombres). Con ruta la guarda.
//        'agregar' los mete en la escena que ya esta (sin cerrarla y sin "SinClips").
//    animsetluajuego <carpeta>
//        reescribe el juego3dmin.lua de 'juego3dmin' para que la franja del HUD se prenda SOLO si
//        el armature "Personaje" tiene sus clips (animClip/animLargo): el binario compilado que
//        no cargue el animset .w3da no pasa juego3dpx (prueba_juego3d_anims.w3s).
//    animsetjuegoquitar <carpeta> quitar|volver
//        saca / devuelve la carpeta animaciones/ de los assets del juego YA compilado
//        (<carpeta>/build/linux/animaciones): sin ella el mismo binario no ve los clips y
//        juego3dpx TIENE que fallar (la prueba del juego se prueba a si misma).
//    animsetpistas <ruta.w3d> <entrada.w3da> [<hueso> N]...
//        las pistas de los clips de UNA entrada .w3da contadas por el NOMBRE de su hueso en la
//        tabla HUES ("-" = sin hueso): cuantas apuntan a cada hueso (un hueso que le falta al
//        esqueleto de una VARIANTE no puede perder sus pistas al guardar).
//    animsetapuntar <origen.w3d> <destino.w3d> <armature> <animset>
//        copia el contenedor reescribiendo su proyecto.json para que ese armature use OTRO animset
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
//        borra ese hueso como la X del Edit Mode de huesos (BoneEditBorrar: remapea las pistas).
//    posefoto <archivo> [juego 0|1] [sin <armature>]...
//        la FOTO de la reproduccion: por cada armature, cada clip y varios frames (inicio,
//        siguiente, medio, final-1, final, pasado el final; con 'juego 1' ademas el cabezal
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
//        los animsets como RECURSOS del proyecto (io/GuardarAnimSets.h, lo que usa el outliner
//        por recursos): usuarios, renombrar, carpeta cosmetica, purgar los huerfanos.
// ============================================================================
#include "test/W3dPruebasAnims.h"
#include "animation/W3dAnimSet.h"
#include "animation/SkeletalAnimation.h"
#include "animation/Animation.h"
#include "objects/Armature.h"
#include "objects/Objects.h"
#include "objects/Collection.h"
#include "io/W3dRecursos.h"
#include "io/W3dZip.h"
#include "io/JsonW3d.h"
#include "io/GuardarAnimSets.h"
#include "edit/BoneEdit.h"            // animsetborrarhueso: BoneEditBorrar (remapea las pistas de los clips)
#include "w3dFilesystem.h"
#include "base/W3dInteractionState.h"  // animsetver/animsetdope: Pose Mode (InteractionMode / estado)
#include "ViewPorts/Timeline.h"        // animsetver/animsetdope: el dope sheet y el editor de curvas de verdad
#include "ViewPorts/ViewPorts.h"       // animsetver: la UI entera dibujada (rootViewport)
#include "undo/Undo.h"                 // animsetdope: el undo de keyframes (tarjeta Keyframe, Ctrl+Z)
#include "test/W3dScript.h"            // animsetver: W3dRunCommand("propstab 9") refresca la tarjeta Keyframe
#include "ViewPorts/Properties.h"      // animsetdope renombrar: los botones Rename de las tarjetas (PropsActivo)
#include "WhiskUI/widgets/TextField.h" // animsetdope renombrar: el campo in-place del rename (g_textFieldActivo)

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

extern void ReiniciarEscena();
extern void CargarTexturasPendientes();   // animsetver: la UI entera se dibuja con sus texturas
extern void RenameCommit();               // animsetdope renombrar: el Enter del rename in-place (Properties.cpp)
extern bool GuardarW3D(const std::string& ruta);
extern Object* FindObjectByName(Object* node, const std::string& name);

namespace {

// el resto de la linea como ruta/nombre (con comillas si tiene espacios)
std::string LeerPalabra(std::istringstream& ss) {
    std::string t;
    if (!(ss >> t)) return std::string();
    if (t.empty() || t[0] != '"') return t;
    std::string r = t.substr(1);
    while (r.empty() || r[r.size() - 1] != '"') {
        std::string mas;
        if (!(ss >> mas)) break;
        r += " " + mas;
    }
    if (!r.empty() && r[r.size() - 1] == '"') r.erase(r.size() - 1);
    return r;
}

// un nombre SIN carpeta va a ../debug/ (la carpeta de salidas de la suite, ignorada por git),
// igual que W3dDebugFile de W3dScript.cpp; una ruta con carpeta queda como esta
std::string RutaDebug(const std::string& n) {
    if (n.find('/') != std::string::npos || n.find('\\') != std::string::npos) return n;
#ifdef _WIN32
    _mkdir("../debug");
#else
    mkdir("../debug", 0755);
#endif
    return "../debug/" + n;
}

// pares "clave valor" del final de la linea (los asserts)
bool LeerAsserts(std::istringstream& ss, std::vector<std::string>& claves, std::vector<std::string>& vals,
                 const char* cmd, std::string& err) {
    std::string k;
    while (ss >> k) {
        std::string v;
        if (!(ss >> v)) { err = std::string(cmd) + ": falta el valor de '" + k + "'"; return false; }
        claves.push_back(k); vals.push_back(v);
    }
    return true;
}

bool AssertInt(const char* cmd, const std::string& k, const std::string& v, long tiene, std::string& err) {
    const long quiere = atol(v.c_str());
    if (quiere == tiene) return true;
    char b[200];
    sprintf(b, "%s: %s = %ld, se esperaba %ld", cmd, k.c_str(), tiene, quiere);
    err = b;
    return false;
}

Armature* ArmPorNombre(const std::string& n) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : 0;
    return (o && o->getType() == ObjectType::armature) ? (Armature*)o : 0;
}

void JuntarArms(Object* o, std::vector<Armature*>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) out.push_back((Armature*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarArms(o->Childrens[i], out);
}

// ============================================================================
//  animsetinfo
// ============================================================================
bool CmdInfo(std::istringstream& ss, std::string& err) {
    W3dAnimSetsStats st;
    W3dAnimSetsEstadisticas(st);
    const std::vector<W3dAnimSetFila>& reg = W3dAnimSetsRegistro();
    std::vector<W3dRecurso*> descs;
    W3dRecursosListar(W3DREC_ANIMSET, descs);
    int enMemoria = 0;
    for (size_t i = 0; i < descs.size(); i++)
        if (descs[i] && descs[i]->id.compare(0, 8, "#memoria") == 0) enMemoria++;
    printf("      [animsetinfo] vivos=%d refs=%d clips=%d variantes=%d clipsVariante=%d keys=%ld KB=%ld copias=%d registro=%d descriptores=%d enmemoria=%d\n",
           st.vivos, st.refs, st.clips, st.variantes, st.clipsVariante, st.keys, st.bytes / 1024,
           W3dAnimSetsCopias(), (int)reg.size(), (int)descs.size(), enMemoria);
    for (size_t i = 0; i < descs.size() && i < 40; i++)
        if (descs[i] && (descs[i]->estado != W3DREC_LISTO || descs[i]->id.empty() || descs[i]->id[0] == '#'))
            printf("      [animsetinfo]   descriptor '%s' estado=%d refs=%d\n", descs[i]->id.c_str(),
                   (int)descs[i]->estado, descs[i]->refTotal);
    for (size_t i = 0; i < reg.size() && i < 40; i++)
        printf("      [animsetinfo]   '%s' -> %s%s%s\n", reg[i].nombre.c_str(), reg[i].entrada.c_str(),
               reg[i].carpeta.empty() ? "" : " carpeta=", reg[i].carpeta.c_str());
    std::vector<std::string> k, v;
    if (!LeerAsserts(ss, k, v, "animsetinfo", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        long tiene = 0;
        if      (k[i] == "vivos")         tiene = st.vivos;
        else if (k[i] == "refs")          tiene = st.refs;
        else if (k[i] == "clips")         tiene = st.clips;
        else if (k[i] == "variantes")     tiene = st.variantes;
        else if (k[i] == "clipsvariante") tiene = st.clipsVariante;
        else if (k[i] == "copias")        tiene = W3dAnimSetsCopias();
        else if (k[i] == "registro")      tiene = (long)reg.size();
        else if (k[i] == "descriptores")  tiene = (long)descs.size();
        else if (k[i] == "enmemoria")     tiene = enMemoria;
        else { err = "animsetinfo: assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt("animsetinfo", k[i], v[i], tiene, err)) return false;
    }
    return true;
}

// ============================================================================
//  animsetrec <id> [refs N] [nada]: el descriptor del almacen con ese id (la entrada)
// ============================================================================
bool CmdRec(std::istringstream& ss, std::string& err) {
    const std::string id = LeerPalabra(ss);
    W3dRecurso* r = W3dRecursoBuscar(W3DREC_ANIMSET, id);
    const W3dAnimSet* set = (r && r->estado == W3DREC_LISTO) ? (const W3dAnimSet*)r->dato : 0;
    if (r) printf("      [animsetrec] '%s': estado=%d refs=%d animset='%s'\n", id.c_str(), (int)r->estado, (int)r->refTotal,
                  set ? set->nombre.c_str() : "");
    else   printf("      [animsetrec] '%s': NADA (ningun recurso con ese id)\n", id.c_str());
    std::string k;
    while (ss >> k) {
        if (k == "nada") { if (r) { err = "animsetrec: hay un recurso con id '" + id + "'"; return false; } continue; }
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
    const std::string n = LeerPalabra(ss);
    Armature* a = ArmPorNombre(n);
    if (!a) { err = "animsetarm: no hay un armature '" + n + "'"; return false; }
    W3dAnimSet* set = W3dArmatureAnimSet(a);
    printf("      [animsetarm] '%s': %d clips %s%s%s%s animSetNombre='%s'%s\n", a->name.c_str(), (int)a->animations.size(),
           set ? "COMPARTIDOS de '" : "PROPIOS", set ? set->nombre.c_str() : "", set ? "'" : "",
           a->animsVar ? " (variante)" : "", a->animSetNombre.c_str(),
           a->animSetNoCargo.empty() ? "" : " NO CARGO");
    std::vector<std::string> k, v;
    if (!LeerAsserts(ss, k, v, "animsetarm", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        if (k[i] == "animset") {
            const std::string tiene = set ? set->nombre : std::string();
            if (tiene != v[i]) { err = "animsetarm: animset = '" + tiene + "', se esperaba '" + v[i] + "'"; return false; }
            continue;
        }
        if (k[i] == "nombreset") {
            if (a->animSetNombre != v[i]) { err = "animsetarm: animSetNombre = '" + a->animSetNombre + "', se esperaba '" + v[i] + "'"; return false; }
            continue;
        }
        long tiene = 0;
        if      (k[i] == "compartido") tiene = set ? 1 : 0;
        else if (k[i] == "clips")      tiene = (long)a->animations.size();
        else if (k[i] == "variante")   tiene = a->animsVar ? 1 : 0;
        else if (k[i] == "nocargo")    tiene = a->animSetNoCargo.empty() ? 0 : 1;
        else { err = "animsetarm: assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt("animsetarm", k[i], v[i], tiene, err)) return false;
    }
    return true;
}

long KeysDe(const SkeletalAnimation* c) {
    long k = 0;
    if (c) W3dAnimSetClipBytes(c, &k);
    return k;
}

bool CmdClip(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    int i = -1; ss >> i;
    Armature* a = ArmPorNombre(n);
    if (!a) { err = "animsetclip: no hay un armature '" + n + "'"; return false; }
    if (i < 0 || i >= (int)a->animations.size() || !a->animations[i]) { err = "animsetclip: el clip no existe"; return false; }
    const SkeletalAnimation* c = a->animations[i];
    printf("      [animsetclip] '%s'[%d] = '%s' fps=%d %d..%d pistas=%d keys=%ld\n", a->name.c_str(), i, c->name.c_str(),
           c->FrameRate, c->startFrame, c->endFrame, (int)c->tracks.size(), KeysDe(c));
    std::string k;
    while (ss >> k) {
        if (k == "nombre") {
            const std::string v = LeerPalabra(ss);
            if (c->name != v) { err = "animsetclip: nombre = '" + c->name + "', se esperaba '" + v + "'"; return false; }
        } else if (k == "keys") {
            long v = -1; ss >> v;
            if (KeysDe(c) != v) { char b[96]; sprintf(b, "animsetclip: keys = %ld, se esperaba %ld", KeysDe(c), v); err = b; return false; }
        } else if (k == "mismo") {
            const std::string n2 = LeerPalabra(ss);
            int j = -1, quiere = -1; ss >> j >> quiere;
            Armature* b2 = ArmPorNombre(n2);
            if (!b2 || j < 0 || j >= (int)b2->animations.size()) { err = "animsetclip: 'mismo' apunta a un clip que no existe"; return false; }
            const int es = (b2->animations[j] == c) ? 1 : 0;
            printf("      [animsetclip]   mismo objeto que '%s'[%d]: %s\n", b2->name.c_str(), j, es ? "SI" : "no");
            if (es != quiere) { err = std::string("animsetclip: 'mismo' = ") + (es ? "1" : "0") + ", se esperaba el otro"; return false; }
        } else { err = "animsetclip: assert desconocido '" + k + "'"; return false; }
    }
    return true;
}

// ============================================================================
//  animsetsw3d: lo que hay adentro de un contenedor
// ============================================================================
void ContarArmaturesJson(JVal* o, int& conSet, int& inl) {
    if (!o || o->tipo != 4) return;
    if (JS(o, "tipo", "") == "armature") {
        if (!JS(o, "anims", "").empty()) conSet++;
        if (JHijo(o, "anims", 5)) inl++;
    }
    JVal* h = JHijo(o, "hijos", 5);
    if (h) for (size_t i = 0; i < h->lista.size(); i++) ContarArmaturesJson(h->lista[i], conSet, inl);
}

bool CmdSw3d(std::istringstream& ss, std::string& err) {
    const std::string ruta = LeerPalabra(ss);
    W3dZipLector z;
    if (!z.Abrir(ruta)) { err = "animsetsw3d: no puedo abrir " + ruta; return false; }
    std::vector<std::string> ents;
    z.Listar(ents);
    std::vector<unsigned char> pj;
    if (!z.Leer("proyecto.json", pj) || pj.empty()) { err = "animsetsw3d: sin proyecto.json"; return false; }
    int nSets = 0, nClips = 0, malas = 0;
    long bytesSets = 0;
    std::set<std::string> w3da;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& e = ents[i];
        if (e.size() < 5 || e.compare(e.size() - 5, 5, ".w3da") != 0) continue;
        w3da.insert(e);
        std::vector<unsigned char> d;
        W3dAnimSetDatos dat;
        std::string motivo;
        if (!z.Leer(e, d) || d.empty() || !W3dAnimSetLeer(&d[0], d.size(), dat, &motivo)) {
            printf("      [animsetsw3d] %s NO SE LEE: %s\n", e.c_str(), motivo.c_str());
            malas++;
            continue;
        }
        long keys = 0;
        for (size_t c = 0; c < dat.clips.size(); c++) W3dAnimSetClipBytes(dat.clips[c], &keys);
        printf("      [animsetsw3d] %s: %d huesos, %d clips, %ld keys, %d B%s\n", e.c_str(), (int)dat.huesos.size(),
               (int)dat.clips.size(), keys, (int)d.size(), dat.ajenos.empty() ? "" : " (+bloques ajenos)");
        nSets++; nClips += (int)dat.clips.size(); bytesSets += (long)d.size();
    }
    JParser p((const char*)&pj[0], pj.size());
    JVal* raiz = p.Valor();
    int conSet = 0, inl = 0, reg = 0, regSinEntrada = 0;
    if (raiz && raiz->tipo == 4) {
        JVal* esc = JHijo(raiz, "escena", 4);
        JVal* objs = esc ? JHijo(esc, "objetos", 5) : 0;
        if (objs) for (size_t i = 0; i < objs->lista.size(); i++) ContarArmaturesJson(objs->lista[i], conSet, inl);
        JVal* ja = JHijo(raiz, "animsets", 5);
        if (ja) for (size_t i = 0; i < ja->lista.size(); i++) {
            reg++;
            const std::string en = JS(ja->lista[i], "entrada", "");
            if (!w3da.count(en)) { regSinEntrada++; printf("      [animsetsw3d] el registro nombra '%s' y no esta\n", en.c_str()); }
        }
    }
    delete raiz;
    printf("      [animsetsw3d] %s: %d animsets (%d clips, %ld KB), registro=%d, armatures con animset=%d, con clips INLINE=%d, proyecto.json=%ld KB\n",
           ruta.c_str(), nSets, nClips, bytesSets / 1024, reg, conSet, inl, (long)pj.size() / 1024);
    if (malas) { err = "animsetsw3d: hay entradas .w3da que no se leen"; return false; }
    if (regSinEntrada) { err = "animsetsw3d: el registro nombra entradas que no estan"; return false; }
    std::vector<std::string> k, v;
    if (!LeerAsserts(ss, k, v, "animsetsw3d", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        if (k[i] == "jsonmaxkb") {
            if ((long)pj.size() > atol(v[i].c_str()) * 1024) { err = "animsetsw3d: proyecto.json pesa mas de " + v[i] + " KB"; return false; }
            continue;
        }
        if (k[i] == "bytesmaxkb") {
            if (bytesSets > atol(v[i].c_str()) * 1024) { err = "animsetsw3d: los .w3da pesan mas de " + v[i] + " KB"; return false; }
            continue;
        }
        long tiene = 0;
        if      (k[i] == "animsets")  tiene = nSets;
        else if (k[i] == "clips")     tiene = nClips;
        else if (k[i] == "inline")    tiene = inl;
        else if (k[i] == "armatures") tiene = conSet;
        else if (k[i] == "registro")  tiene = reg;
        else { err = "animsetsw3d: assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt("animsetsw3d", k[i], v[i], tiene, err)) return false;
    }
    return true;
}

// ============================================================================
//  animsetsintetica
// ============================================================================
const char* kHuesos[5] = { "Raiz", "Cadera", "Torso", "Cabeza", "Brazo" };
const int   kPadres[5] = { -1, 0, 1, 2, 2 };

int HuesoDe(const Armature* a, const char* n) {
    for (size_t i = 0; i < a->bones.size(); i++) if (a->bones[i].name == n) return (int)i;
    return -1;
}

Armature* CrearRig(Object* padre, const std::string& nombre, bool invertido, float x) {
    Armature* a = new Armature(padre, Vector3(x, 0, 0));
    a->SetNameObj(nombre);
    // orden de los huesos: el natural o uno permutado (mismos nombres, otros indices)
    const int ordenN[5] = { 0, 1, 2, 3, 4 };
    const int ordenI[5] = { 4, 2, 0, 3, 1 };
    const int* orden = invertido ? ordenI : ordenN;
    int indiceDe[5];
    for (int k = 0; k < 5; k++) indiceDe[orden[k]] = k;
    for (int k = 0; k < 5; k++) {
        const int h = orden[k];
        W3dBone b;
        b.name = kHuesos[h];
        b.parent = kPadres[h] < 0 ? -1 : indiceDe[kPadres[h]];
        b.head = Vector3(0.0f, (float)h * 0.5f, 0.0f);
        b.tail = Vector3(0.0f, (float)h * 0.5f + 0.4f, 0.1f * (float)h);
        a->bones.push_back(b);
    }
    PrepararSkinAutorado(a);
    return a;
}

// un clip determinista que cubre los modos del formato
SkeletalAnimation* ClipSintetico(const Armature* a, const std::string& nombre, int semilla) {
    SkeletalAnimation* c = new SkeletalAnimation(nombre);
    c->FrameRate = 24 + semilla % 7;
    c->startFrame = 1;
    c->endFrame = 30 + semilla;
    for (int h = 0; h < 5; h++) {
        BoneTrack tr;
        tr.bone = HuesoDe(a, kHuesos[h]);
        // rotacion HORNEADA: un key por frame, lineal, sin handles (el caso de las anims importadas)
        for (int comp = 0; comp < 3; comp++) {
            AnimProperty ap; ap.Property = AnimRotation; ap.component = comp;
            for (int f = c->startFrame; f <= c->endFrame; f++) {
                keyFrame k; k.frame = f; k.Interpolation = KfLinear; k.handleType = HAuto;
                k.value = (float)(20.0 * sin(0.21 * f + 0.7 * h + 0.3 * comp + 0.11 * semilla));
                ap.keyframes.push_back(k);
            }
            tr.Propertys.push_back(ap);
        }
        if (h == 0) {
            // posicion de la raiz: BEZIER con handles libres, frames IRREGULARES, interpolaciones mezcladas
            for (int comp = 0; comp < 3; comp++) {
                AnimProperty ap; ap.Property = AnimPosition; ap.component = comp;
                const int fr[6] = { 1, 4, 5, 11, 20, 29 };
                for (int q = 0; q < 6; q++) {
                    keyFrame k; k.frame = fr[q];
                    k.value = (float)(0.37 * q * (comp + 1) + 0.01 * semilla);
                    k.Interpolation = (q % 3 == 0) ? KfBezier : (q % 3 == 1 ? KfLinear : KfConstant);
                    k.handleType = (q % 2) ? HFree : HAligned;
                    k.inDF = -1.5f - 0.1f * q; k.inDV = (q == 2) ? -0.0f : 0.25f * q;
                    k.outDF = 1.25f; k.outDV = -0.125f * q;
                    ap.keyframes.push_back(k);
                }
                tr.Propertys.push_back(ap);
            }
            // escala: UN key (curva de un solo keyframe)
            AnimProperty es; es.Property = AnimScale; es.component = AnimY;
            keyFrame k; k.frame = 7; k.value = 1.5f; es.keyframes.push_back(k);
            tr.Propertys.push_back(es);
            // una curva VACIA (sin keyframes) tambien viaja
            AnimProperty vacia; vacia.Property = AnimScale; vacia.component = AnimZ;
            tr.Propertys.push_back(vacia);
        }
        c->tracks.push_back(tr);
    }
    // una pista SIN hueso (inerte) y una REPETIDA para la cabeza (el FK usa la primera)
    { BoneTrack tr; tr.bone = -1;
      AnimProperty ap; ap.Property = AnimRotation; ap.component = AnimX;
      keyFrame k; k.frame = 3; k.value = 45.0f; ap.keyframes.push_back(k);
      tr.Propertys.push_back(ap); c->tracks.push_back(tr); }
    { BoneTrack tr; tr.bone = HuesoDe(a, "Cabeza");
      AnimProperty ap; ap.Property = AnimRotation; ap.component = AnimZ;
      keyFrame k; k.frame = 2; k.value = -90.0f; ap.keyframes.push_back(k);
      tr.Propertys.push_back(ap); c->tracks.push_back(tr); }
    return c;
}

bool CmdSintetica(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 1) { err = "animsetsintetica: uso: animsetsintetica <n> [ruta.w3d] [invertido]"; return false; }
    std::string ruta = LeerPalabra(ss), extra;
    bool invertido = false, agregar = false;
    if (ruta == "invertido") { invertido = true; ruta.clear(); }
    if (ruta == "agregar")   { agregar = true; ruta.clear(); }
    while (ss >> extra) { if (extra == "invertido") invertido = true; if (extra == "agregar") agregar = true; }
    // 'agregar': a la escena que ya esta (sin cerrarla), para meter personajes en otra prueba
    if (!agregar) ReiniciarEscena();
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Personajes");
    if (!agregar) CollectionActive = col;
    const int total = n + (invertido ? 1 : 0);
    const char* nombres[4] = { "Quieto", "Caminar", "Correr", "Saltar" };
    for (int i = 0; i < total; i++) {
        const bool inv = (i == n);
        Armature* a = CrearRig(col, inv ? "Invertido" : "Personaje", inv, 2.0f * (float)i);
        for (int c = 0; c < 4; c++) {
            // el segundo personaje tiene su PROPIO "Quieto" (semilla distinta): no se deduplica
            const int semilla = (c == 0 && i == 1) ? 50 : c;
            a->animations.push_back(ClipSintetico(a, nombres[c], semilla));
        }
        // el tercero usa DOS VECES el mismo clip (igual contenido y nombre)
        if (i == 2) a->animations.push_back(ClipSintetico(a, nombres[1], 1));
        a->animActiva = 0;
    }
    // uno sin clips (no aparece en ningun animset)
    if (!agregar) { Armature* a = CrearRig(col, "SinClips", false, -2.0f); (void)a; }
    DeseleccionarTodo();
    ObjActivo = NULL;
    printf("      [animsetsintetica] %d armatures del mismo rig con 4 clips%s\n", n, invertido ? " + uno con los huesos en otro orden" : "");
    if (!ruta.empty()) {
        const bool ok = GuardarW3D(ruta);
        const GuardarAnimSetsInfo& gi = GuardarAnimSetsUltimo();
        printf("      [animsetsintetica] guardada en %s -> %s (animsets=%d clips %d -> %d, %ld B)\n", ruta.c_str(),
               ok ? "OK" : "FALLO", gi.animsets, gi.clipsArmatures, gi.clipsEscritos, gi.bytes);
        if (!ok) { err = "animsetsintetica: no pude guardar " + ruta; return false; }
    }
    return true;
}

// ============================================================================
//  animsetapuntar: reescribe el "anims" de un armature adentro del contenedor
// ============================================================================
bool CmdApuntar(std::istringstream& ss, std::string& err) {
    const std::string ruta = LeerPalabra(ss), dest = LeerPalabra(ss), arm = LeerPalabra(ss), set = LeerPalabra(ss);
    if (ruta.empty() || dest.empty() || arm.empty() || set.empty()) { err = "animsetapuntar: uso: animsetapuntar <origen.w3d> <destino.w3d> <armature> <animset>"; return false; }
    std::vector<W3dZipEntrada> ents;
    if (!W3dZipLeer(ruta, &ents)) { err = "animsetapuntar: no puedo leer " + ruta; return false; }
    bool hecho = false;
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].nombre != "proyecto.json") continue;
        std::string t(ents[i].datos.begin(), ents[i].datos.end());
        const size_t pn = t.find("\"nombre\": \"" + arm + "\"");
        if (pn == std::string::npos) break;
        const std::string clave = "\"anims\": \"";
        const size_t pa = t.find(clave, pn);
        if (pa == std::string::npos) break;
        const size_t ini = pa + clave.size(), fin = t.find('"', ini);
        if (fin == std::string::npos) break;
        t.replace(ini, fin - ini, set);
        ents[i].datos.assign(t.begin(), t.end());
        hecho = true;
    }
    if (!hecho) { err = "animsetapuntar: no encontre el \"anims\" de '" + arm + "'"; return false; }
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].nombre == "mimetype") w.Agregar(ents[i].nombre, ents[i].datos);
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].nombre != "mimetype") w.Agregar(ents[i].nombre, ents[i].datos);
    if (!w.Guardar(dest)) { err = "animsetapuntar: no pude escribir " + dest; return false; }
    printf("      [animsetapuntar] '%s' ahora usa el animset '%s'\n", arm.c_str(), set.c_str());
    return true;
}

// ============================================================================
//  animsetrenombrar / animsetborrar
// ============================================================================
bool CmdRenombrar(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    int i = -1; ss >> i;
    const std::string nuevo = LeerPalabra(ss);
    Armature* a = ArmPorNombre(n);
    if (!a || i < 0 || i >= (int)a->animations.size() || nuevo.empty()) { err = "animsetrenombrar: uso: animsetrenombrar <armature> <i> <nombre>"; return false; }
    W3dArmatureAnimsPropias(a);   // lo mismo que hace el rename del panel antes de tomar el puntero
    a->animations[i]->name = nuevo;
    printf("      [animsetrenombrar] '%s'[%d] -> '%s'\n", a->name.c_str(), i, nuevo.c_str());
    return true;
}

bool CmdLuaJuego(std::istringstream& ss, std::string& err) {
    const std::string dir = LeerPalabra(ss);
    if (dir.empty()) { err = "animsetluajuego: uso: animsetluajuego <carpeta>"; return false; }
    FILE* f = fopen((dir + "/juego3dmin.lua").c_str(), "w");
    if (!f) { err = "animsetluajuego: no pude escribir " + dir + "/juego3dmin.lua"; return false; }
    fputs("-- juego 3D minimo + ANIMSETS: la franja se prende solo si los clips del armature cargaron\n"
          "function inicio() end\n"
          "function actualizar(dt)\n"
          "  local fx, fz, rx, rz = camaraXZ()\n"
          "  local w, h = pantalla()\n"
          "  local v = parametro(\"loquesea\", 7)\n"
          "  local p = buscar(\"Personaje\")\n"
          "  local q = buscar(\"Personaje.002\")\n"
          "  local ok = p and q and animClip(p, \"Correr\") and animLargo(p, \"Correr\") == 32\n"
          "            and animLargo(q, \"Caminar\") == 31 and animLargo(p, \"NoExiste\") == 0\n"
          "  if fx and w and v == 7 and ok then mostrar(objeto(\"franja\"), true) end\n"
          "end\n", f);
    fclose(f);
    printf("      [animsetluajuego] %s/juego3dmin.lua\n", dir.c_str());
    return true;
}

// saca / devuelve la carpeta animaciones/ de los assets del juego YA compilado (el staging suelto
// de 'compilarjuego 0 0'). Va por la shell como juego3dpx: la prueba del juego es de Linux.
bool CmdJuegoQuitar(std::istringstream& ss, std::string& err) {
    const std::string dir = LeerPalabra(ss);
    std::string modo; ss >> modo;
    if (dir.empty() || (modo != "quitar" && modo != "volver") || dir.find('\'') != std::string::npos) {
        err = "animsetjuegoquitar: uso: animsetjuegoquitar <carpeta> quitar|volver"; return false;
    }
    const std::string base = dir + "/build/linux/animaciones";
    struct stat st;
    std::string sh;
    if (modo == "quitar") {
        if (stat(base.c_str(), &st) != 0) { err = "animsetjuegoquitar: el juego no tiene " + base; return false; }
        // un '.quitada' de una corrida anterior que se corto no puede frenar el mv
        sh = "rm -rf '" + base + ".quitada' && mv '" + base + "' '" + base + ".quitada'";
    } else {
        if (stat((base + ".quitada").c_str(), &st) != 0) { err = "animsetjuegoquitar: no hay nada que devolver (" + base + ".quitada)"; return false; }
        sh = "rm -rf '" + base + "' && mv '" + base + ".quitada' '" + base + "'";
    }
    if (system(sh.c_str()) != 0) { err = "animsetjuegoquitar: fallo '" + sh + "'"; return false; }
    printf("      [animsetjuegoquitar] %s: %s\n", base.c_str(), modo == "quitar" ? "FUERA (el juego no tiene sus .w3da)" : "de vuelta");
    return true;
}

// las pistas de UNA entrada .w3da, contadas por el nombre de su hueso en la tabla
bool CmdPistas(std::istringstream& ss, std::string& err) {
    const std::string ruta = LeerPalabra(ss), ent = LeerPalabra(ss);
    if (ruta.empty() || ent.empty()) { err = "animsetpistas: uso: animsetpistas <ruta.w3d> <entrada.w3da> [<hueso> N]..."; return false; }
    W3dZipLector z;
    if (!z.Abrir(ruta)) { err = "animsetpistas: no puedo abrir " + ruta; return false; }
    std::vector<unsigned char> d;
    W3dAnimSetDatos dat;
    std::string motivo;
    if (!z.Leer(ent, d) || d.empty() || !W3dAnimSetLeer(&d[0], d.size(), dat, &motivo)) {
        err = "animsetpistas: '" + ent + "' no esta o no se lee " + motivo; return false;
    }
    std::map<std::string, long> porHueso;
    for (size_t c = 0; c < dat.clips.size(); c++) {
        const SkeletalAnimation* cl = dat.clips[c];
        if (!cl) continue;
        for (size_t t = 0; t < cl->tracks.size(); t++) {
            const int b = cl->tracks[t].bone;
            porHueso[(b >= 0 && b < (int)dat.huesos.size()) ? dat.huesos[b] : std::string("-")]++;
        }
    }
    std::string lista;
    for (std::map<std::string, long>::const_iterator it = porHueso.begin(); it != porHueso.end(); ++it) {
        char b[32]; sprintf(b, "=%ld", it->second);
        lista += (lista.empty() ? "" : " ") + it->first + b;
    }
    printf("      [animsetpistas] %s %s: %d clips, pistas por hueso: %s\n", ruta.c_str(), ent.c_str(),
           (int)dat.clips.size(), lista.c_str());
    std::vector<std::string> k, v;
    if (!LeerAsserts(ss, k, v, "animsetpistas", err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        std::map<std::string, long>::const_iterator it = porHueso.find(k[i]);
        if (!AssertInt("animsetpistas", k[i], v[i], it == porHueso.end() ? 0 : it->second, err)) return false;
    }
    return true;
}

bool CmdActivo(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    Armature* a = ArmPorNombre(n);
    if (!a) { err = "animsetactivo: no hay un armature '" + n + "'"; return false; }
    DeseleccionarTodo();
    ObjActivo = a;
    printf("      [animsetactivo] '%s'\n", a->name.c_str());
    return true;
}

// renombra un hueso SIN tocar los clips, como el rename de un vertex group que arrastra al hueso
// homonimo del rig (Properties / MeshEdit): el armature sigue colgado de su animset
bool CmdRenombrarHueso(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss), viejo = LeerPalabra(ss), nuevo = LeerPalabra(ss);
    Armature* a = ArmPorNombre(n);
    const int i = a ? HuesoDe(a, viejo.c_str()) : -1;
    if (!a || i < 0 || nuevo.empty()) { err = "animsetrenombrarhueso: uso: animsetrenombrarhueso <armature> <hueso> <nuevo>"; return false; }
    a->bones[i].name = nuevo;
    printf("      [animsetrenombrarhueso] '%s': '%s' -> '%s' (clips %s)\n", a->name.c_str(), viejo.c_str(), nuevo.c_str(),
           W3dArmatureAnimsCompartidas(a) ? "siguen COMPARTIDOS" : "propios");
    return true;
}

bool CmdBorrarHueso(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss), h = LeerPalabra(ss);
    Armature* a = ArmPorNombre(n);
    const int i = a ? HuesoDe(a, h.c_str()) : -1;
    if (!a || i < 0) { err = "animsetborrarhueso: uso: animsetborrarhueso <armature> <hueso>"; return false; }
    for (size_t k = 0; k < a->bones.size(); k++) a->bones[k].select = false;
    a->bones[i].select = true; a->boneActivo = i;
    const bool ok = BoneEditBorrar(a);
    printf("      [animsetborrarhueso] '%s' sin '%s': %s (%d huesos)\n", a->name.c_str(), h.c_str(), ok ? "OK" : "FALLO", (int)a->bones.size());
    if (!ok) { err = "animsetborrarhueso: BoneEditBorrar no borro nada"; return false; }
    return true;
}

bool CmdBorrar(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : 0;
    if (!o || o == SceneCollection) { err = "animsetborrar: no hay un objeto '" + n + "'"; return false; }
    Object* padre = o->Parent ? o->Parent : SceneCollection;
    for (size_t i = 0; i < padre->Childrens.size(); i++)
        if (padre->Childrens[i] == o) { padre->Childrens.erase(padre->Childrens.begin() + i); break; }
    if (ObjActivo == o) ObjActivo = NULL;
    DeseleccionarTodo();
    // ActiveAnimKind / ActiveAnimArm: animation/Animation.h
    if (ActiveAnimArm == (Armature*)o) { ActiveAnimArm = NULL; ActiveAnimKind = 0; }
    W3dLiberarSubarbol(o);
    printf("      [animsetborrar] '%s' borrado\n", n.c_str());
    return true;
}

// ============================================================================
//  posefoto / posefotoigual
// ============================================================================
void PonBits(std::string& s, const float* f, int n) { s.append((const char*)f, (size_t)n * sizeof(float)); }

std::string HashPose(const Armature* a) {
    std::string s;
    for (size_t i = 0; i < a->bones.size(); i++) {
        const W3dBone& b = a->bones[i];
        const float t[15] = { b.poseT.x, b.poseT.y, b.poseT.z, b.poseR.x, b.poseR.y, b.poseR.z,
                              b.poseS.x, b.poseS.y, b.poseS.z, b.poseHead.x, b.poseHead.y, b.poseHead.z,
                              b.poseTail.x, b.poseTail.y, b.poseTail.z };
        PonBits(s, t, 15);
        PonBits(s, b.poseWorld.m, 16);
        PonBits(s, b.skinMatrix.m, 16);
    }
    unsigned h1, h2;
    W3dAnimSetHuella((const unsigned char*)s.data(), s.size(), h1, h2);
    char b[40]; sprintf(b, "%08x%08x", h1, h2);
    return b;
}

bool CmdPoseFoto(std::istringstream& ss, std::string& err) {
    const std::string nom = LeerPalabra(ss);
    const std::string ruta = nom.empty() ? nom : RutaDebug(nom);
    std::string k; int juego = 0;
    // 'sin X': X no entra en la foto; 'solo X': SOLO los nombrados (fotografiar a los demas / al editado)
    std::set<std::string> sin, solo;
    while (ss >> k) {
        if (k == "juego") ss >> juego;
        else if (k == "sin") sin.insert(LeerPalabra(ss));
        else if (k == "solo") solo.insert(LeerPalabra(ss));
        else { err = "posefoto: opcion desconocida '" + k + "'"; return false; }
    }
    if (ruta.empty()) { err = "posefoto: uso: posefoto <archivo> [juego 0|1] [sin <armature>]... [solo <armature>]..."; return false; }
    std::vector<Armature*> arms;
    JuntarArms(SceneCollection, arms);
    for (size_t i = arms.size(); i-- > 0; )
        if (sin.count(arms[i]->name) || (!solo.empty() && !solo.count(arms[i]->name))) arms.erase(arms.begin() + i);
    // ActiveAnimKind / ActiveAnimArm: animation/Animation.h
    const int kind0 = ActiveAnimKind; Armature* arm0 = ActiveAnimArm;
    std::string out;
    int nPoses = 0, nClips = 0;
    for (size_t ai = 0; ai < arms.size(); ai++) {
        Armature* a = arms[ai];
        const int activa0 = a->animActiva; const float jf0 = a->juegoFrame;
        for (size_t ci = 0; ci < a->animations.size(); ci++) {
            SkeletalAnimation* c = a->animations[ci];
            if (!c) continue;
            nClips++;
            a->animActiva = (int)ci;
            const int ini = c->startFrame, fin = c->endFrame, medio = (ini + fin) / 2;
            const int fr[6] = { ini, ini + 1, medio, fin - 1, fin, fin + 5 };
            for (int q = 0; q < 6; q++) {
                ActiveAnimKind = 1; ActiveAnimArm = a;
                a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = false;
                EvaluarPoseEsqueleto(a, fr[q]);
                char b[64]; sprintf(b, "|%d|f%d|", (int)ci, fr[q]);
                out += a->name + b + c->name + "|" + HashPose(a) + "\n";
                nPoses++;
            }
            if (juego) {
                const float largo = (float)(fin - ini);
                const float jf[4] = { 0.0f, 0.37f, largo * 0.5f + 0.5f, largo > 0.0f ? largo : 0.0f };
                for (int q = 0; q < 4; q++) {
                    ActiveAnimKind = 2; ActiveAnimArm = 0;
                    a->juegoFrame = jf[q];
                    a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->lastPoseFrameF = -999999.0f; a->poseDirty = false;
                    EvaluarPoseEsqueleto(a, 0);
                    char b[80]; sprintf(b, "|%d|j%.3f|", (int)ci, jf[q]);
                    out += a->name + b + c->name + "|" + HashPose(a) + "\n";
                    nPoses++;
                }
            }
        }
        a->animActiva = activa0; a->juegoFrame = jf0;
        a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = false;
    }
    ActiveAnimKind = kind0; ActiveAnimArm = arm0;
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) { err = "posefoto: no puedo escribir " + ruta; return false; }
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    unsigned h1, h2;
    W3dAnimSetHuella((const unsigned char*)out.data(), out.size(), h1, h2);
    printf("      [posefoto] %s: %d armatures, %d clips, %d poses, huella %08x%08x\n", ruta.c_str(), (int)arms.size(), nClips, nPoses, h1, h2);
    return true;
}

bool LeerLineas(const std::string& ruta, std::vector<std::string>& out) {
    out.clear();
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(ruta, d)) return false;
    std::string cur;
    for (size_t i = 0; i < d.size(); i++) {
        if (d[i] == '\n') { out.push_back(cur); cur.clear(); } else cur += (char)d[i];
    }
    if (!cur.empty()) out.push_back(cur);
    return true;
}

bool CmdPoseFotoIgual(std::istringstream& ss, std::string& err) {
    const std::string a = RutaDebug(LeerPalabra(ss)), b = RutaDebug(LeerPalabra(ss));
    std::vector<std::string> la, lb;
    if (!LeerLineas(a, la) || !LeerLineas(b, lb)) { err = "posefotoigual: no puedo leer las fotos"; return false; }
    int distintas = 0;
    const size_t n = la.size() < lb.size() ? la.size() : lb.size();
    for (size_t i = 0; i < n; i++)
        if (la[i] != lb[i]) {
            if (distintas < 5) printf("      [posefotoigual] DISTINTA:\n        %s\n        %s\n", la[i].c_str(), lb[i].c_str());
            distintas++;
        }
    printf("      [posefotoigual] %d contra %d poses, %d distintas\n", (int)la.size(), (int)lb.size(), distintas);
    if (la.empty()) { err = "posefotoigual: la foto esta vacia (no hay nada que comparar)"; return false; }
    if (la.size() != lb.size() || distintas) { err = "posefotoigual: las fotos NO son identicas"; return false; }
    return true;
}

// ============================================================================
//  animsetromper
// ============================================================================
bool CmdRomper(std::istringstream& ss, std::string& err) {
    const std::string orig = LeerPalabra(ss), dest = LeerPalabra(ss);
    std::string modo; ss >> modo;
    if (orig.empty() || dest.empty() || (modo != "corte" && modo != "version" && modo != "req" && modo != "falta")) {
        err = "animsetromper: uso: animsetromper <origen.w3d> <destino.w3d> corte|version|req|falta"; return false;
    }
    std::vector<W3dZipEntrada> ents;
    if (!W3dZipLeer(orig, &ents)) { err = "animsetromper: no puedo leer " + orig; return false; }
    std::string rota;
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].nombre == "mimetype") w.Agregar(ents[i].nombre, ents[i].datos);
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].nombre == "mimetype") continue;
        const std::string& n = ents[i].nombre;
        std::vector<unsigned char> d = ents[i].datos;
        if (rota.empty() && n.size() > 5 && n.compare(n.size() - 5, 5, ".w3da") == 0 && d.size() > 40) {
            rota = n;
            if (modo == "falta") continue;
            if (modo == "corte") d.resize(d.size() / 2);
            if (modo == "version") d[4] = 9;
            if (modo == "req") { d[40] = 'Z'; d[41] = 'Z'; d[42] = 'Z'; d[43] = 'Z'; }   // el primer fourcc de REQ
        }
        w.Agregar(n, d);
    }
    if (rota.empty()) { err = "animsetromper: el contenedor no trae ninguna .w3da"; return false; }
    if (!w.Guardar(dest)) { err = "animsetromper: no pude escribir " + dest; return false; }
    printf("      [animsetromper] %s: '%s' %s\n", dest.c_str(), rota.c_str(), modo.c_str());
    return true;
}

// ============================================================================
//  animsetcorrupto: robustez del lector
// ============================================================================
unsigned gAzar = 12345u;
unsigned Azar() { gAzar = gAzar * 1103515245u + 12345u; return (gAzar >> 16) & 0x7fffu; }

bool CmdCorrupto(std::string& err) {
    // un animset de verdad: el rig sintetico con sus 4 clips (todos los modos del formato)
    Armature* a = CrearRig(NULL, "RigCorrupto", false, 0.0f);
    const char* nombres[4] = { "Quieto", "Caminar", "Correr", "Saltar" };
    for (int c = 0; c < 4; c++) a->animations.push_back(ClipSintetico(a, nombres[c], c));
    std::vector<std::string> huesos; W3dArmatureNombresHuesos(a, huesos);
    std::vector<int> mapa; W3dAnimSetMapaDesdeArmature(huesos, a, mapa);
    std::vector<std::string> bloques(a->animations.size());
    std::vector<const std::string*> ptrs;
    for (size_t c = 0; c < a->animations.size(); c++) {
        W3dAnimSetClipBloque(*a->animations[c], mapa, (int)huesos.size(), bloques[c]);
        ptrs.push_back(&bloques[c]);
    }
    std::string bien;
    W3dAnimSetArmar(huesos, ptrs, NULL, bien);
    // quitarlo de la escena (el constructor lo colgo de SceneCollection y lo selecciono)
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            if (SceneCollection->Childrens[i] == a) { SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + i); break; }
    DeseleccionarTodo();
    if (ObjActivo == a) ObjActivo = NULL;
    int fallas = 0;
    // 1) lo bueno se lee y se reescribe IGUAL (round-trip de bytes)
    {
        W3dAnimSetDatos d; std::string m;
        if (!W3dAnimSetLeer((const unsigned char*)bien.data(), bien.size(), d, &m)) { printf("      [animsetcorrupto] el bueno no se lee: %s\n", m.c_str()); fallas++; }
        else {
            std::vector<int> id(d.huesos.size()); for (size_t i = 0; i < id.size(); i++) id[i] = (int)i;
            std::vector<std::string> b2(d.clips.size()); std::vector<const std::string*> p2;
            for (size_t c = 0; c < d.clips.size(); c++) { W3dAnimSetClipBloque(*d.clips[c], id, (int)d.huesos.size(), b2[c]); p2.push_back(&b2[c]); }
            std::string otra; W3dAnimSetArmar(d.huesos, p2, &d.ajenos, otra);
            if (otra != bien) { printf("      [animsetcorrupto] leer + escribir NO da los mismos bytes\n"); fallas++; }
            // y los clips son IGUALES campo a campo a los originales (incluidos los bits de los floats)
            for (size_t c = 0; c < d.clips.size() && c < a->animations.size(); c++) {
                const SkeletalAnimation& x = *a->animations[c]; const SkeletalAnimation& y = *d.clips[c];
                bool igual = x.name == y.name && x.FrameRate == y.FrameRate && x.startFrame == y.startFrame &&
                             x.endFrame == y.endFrame && x.tracks.size() == y.tracks.size();
                for (size_t t = 0; igual && t < x.tracks.size(); t++) {
                    const BoneTrack& tx = x.tracks[t]; const BoneTrack& ty = y.tracks[t];
                    igual = tx.bone == ty.bone && tx.Propertys.size() == ty.Propertys.size();
                    for (size_t p = 0; igual && p < tx.Propertys.size(); p++) {
                        const AnimProperty& px = tx.Propertys[p]; const AnimProperty& py = ty.Propertys[p];
                        igual = px.Property == py.Property && px.component == py.component && px.keyframes.size() == py.keyframes.size();
                        for (size_t q = 0; igual && q < px.keyframes.size(); q++)
                            igual = memcmp(&px.keyframes[q], &py.keyframes[q], sizeof(keyFrame)) == 0;
                    }
                }
                if (!igual) { printf("      [animsetcorrupto] el clip %d no vuelve igual\n", (int)c); fallas++; }
            }
        }
    }
    // 2) TODOS los cortes: false, sin clips a medias. La UNICA excepcion es cortar el relleno del
    //    ultimo bloque (la spec lo permite): ahi se acepta y tiene que dar EXACTAMENTE lo mismo.
    int rechazos = 0, sinRelleno = 0;
    for (size_t n = 0; n < bien.size(); n++) {
        W3dAnimSetDatos d; std::string m;
        if (W3dAnimSetLeer((const unsigned char*)bien.data(), n, d, &m)) {
            std::vector<int> id(d.huesos.size()); for (size_t i = 0; i < id.size(); i++) id[i] = (int)i;
            std::vector<std::string> b2(d.clips.size()); std::vector<const std::string*> p2;
            for (size_t c = 0; c < d.clips.size(); c++) { W3dAnimSetClipBloque(*d.clips[c], id, (int)d.huesos.size(), b2[c]); p2.push_back(&b2[c]); }
            std::string otra; W3dAnimSetArmar(d.huesos, p2, &d.ajenos, otra);
            if (n + 4 > bien.size() && otra == bien) sinRelleno++;
            else { printf("      [animsetcorrupto] cortado en %d se ACEPTO\n", (int)n); fallas++; }
        }
        else if (!d.clips.empty() || !d.huesos.empty()) { printf("      [animsetcorrupto] cortado en %d dejo datos a medias\n", (int)n); fallas++; }
        else rechazos++;
    }
    // 3) bytes al azar (no se puede caer; si acepta, los indices de hueso tienen que ser validos)
    int aceptados = 0, azar = 0;
    for (int it = 0; it < 3000; it++) {
        std::string x = bien;
        const int nb = 1 + (int)(Azar() % 6);
        for (int q = 0; q < nb; q++) x[20 + Azar() % (x.size() - 20)] = (char)(Azar() & 255);
        W3dAnimSetDatos d; std::string m;
        if (W3dAnimSetLeer((const unsigned char*)x.data(), x.size(), d, &m)) {
            aceptados++;
            for (size_t c = 0; c < d.clips.size(); c++)
                for (size_t t = 0; t < d.clips[c]->tracks.size(); t++) {
                    const int b = d.clips[c]->tracks[t].bone;
                    if (b < -1 || b >= (int)d.huesos.size()) { printf("      [animsetcorrupto] acepto una pista con hueso %d\n", b); fallas++; }
                }
        } else if (!d.clips.empty()) { printf("      [animsetcorrupto] rechazo dejando clips\n"); fallas++; }
        azar++;
    }
    // 4) version nueva y REQ desconocido: rechazo con motivo
    {
        std::string x = bien; x[4] = 2;
        W3dAnimSetDatos d; std::string m;
        if (W3dAnimSetLeer((const unsigned char*)x.data(), x.size(), d, &m) || m.find("version") == std::string::npos) {
            printf("      [animsetcorrupto] version nueva: no se rechazo por version (%s)\n", m.c_str()); fallas++; }
        std::string y = bien; y[40] = 'Q'; y[41] = 'Q'; y[42] = 'Q'; y[43] = 'Q';
        W3dAnimSetDatos d2; std::string m2;
        if (W3dAnimSetLeer((const unsigned char*)y.data(), y.size(), d2, &m2) || m2.find("QQQQ") == std::string::npos) {
            printf("      [animsetcorrupto] REQ desconocido: no se rechazo por el bloque (%s)\n", m2.c_str()); fallas++; }
    }
    // 5) bloque OPCIONAL desconocido: se acepta, se PRESERVA y se reescribe igual
    {
        std::vector<W3dAnimSetBloqueAjeno> aj(1);
        aj[0].id = "XTRA"; aj[0].dominio = 0; aj[0].cantidad = 3; aj[0].datos = "abcde";
        std::string conAjeno; W3dAnimSetArmar(huesos, ptrs, &aj, conAjeno);
        W3dAnimSetDatos d; std::string m;
        if (!W3dAnimSetLeer((const unsigned char*)conAjeno.data(), conAjeno.size(), d, &m) || d.ajenos.size() != 1 ||
            d.ajenos[0].id != "XTRA" || d.ajenos[0].datos != "abcde" || d.ajenos[0].cantidad != 3) {
            printf("      [animsetcorrupto] el bloque opcional no se preservo (%s)\n", m.c_str()); fallas++;
        } else {
            std::vector<std::string> b2(d.clips.size()); std::vector<const std::string*> p2;
            std::vector<int> id(d.huesos.size()); for (size_t i = 0; i < id.size(); i++) id[i] = (int)i;
            for (size_t c = 0; c < d.clips.size(); c++) { W3dAnimSetClipBloque(*d.clips[c], id, (int)d.huesos.size(), b2[c]); p2.push_back(&b2[c]); }
            std::string otra; W3dAnimSetArmar(d.huesos, p2, &d.ajenos, otra);
            if (otra != conAjeno) { printf("      [animsetcorrupto] con el bloque opcional no se reescribe igual\n"); fallas++; }
        }
    }
    delete a;
    printf("      [animsetcorrupto] %d B: %d cortes rechazados (%d solo sin el relleno final: iguales), %d al azar (%d aceptados sin romper nada), %d falla(s)\n",
           (int)bien.size(), rechazos, sinRelleno, azar, aceptados, fallas);
    if (fallas) { err = "animsetcorrupto: el lector .w3da no es robusto (ver arriba)"; return false; }
    return true;
}

// ============================================================================
//  animsetregistro: el codigo, formato/bloques.tsv (filas w3da:) y formato/w3da.md
//  dicen lo mismo. formato/ es gitignoreado: sin la carpeta se saltea ("SIN CORPUS:",
//  como w3dbregistro; W3D_CORPUS_OBLIGATORIO=1 la exige).
// ============================================================================
bool LeerTexto(const std::string& ruta, std::string& out) {
    std::vector<unsigned char> b;
    if (!w3dFileSystem::ReadFileBytes(ruta, b)) return false;
    out.assign(b.empty() ? "" : (const char*)&b[0], b.size());
    return true;
}
bool CmdRegistro(std::string& err) {
    static const char* kBases[] = { "formato", "../formato", "../../formato", "../../../formato", "../../../../formato", 0 };
    std::string base;
    for (int i = 0; kBases[i] && base.empty(); i++)
        if (w3dFileSystem::FileExists(std::string(kBases[i]) + "/bloques.tsv")) base = kBases[i];
    if (base.empty()) {
        const char* obl = getenv("W3D_CORPUS_OBLIGATORIO");
        if (obl && *obl && strcmp(obl, "0") != 0) { err = "animsetregistro: no encuentro formato/bloques.tsv"; return false; }
        printf("      [animsetregistro] SIN CORPUS: salteo el cruce con formato/bloques.tsv y formato/w3da.md\n");
        return true;
    }
    std::string tsv, md;
    if (!LeerTexto(base + "/bloques.tsv", tsv)) { err = "animsetregistro: no pude leer bloques.tsv"; return false; }
    if (!LeerTexto(base + "/w3da.md", md))      { err = "animsetregistro: no pude leer w3da.md"; return false; }
    std::set<std::string> registrados;
    size_t i = 0;
    while (i < tsv.size()) {
        size_t j = tsv.find('\n', i); if (j == std::string::npos) j = tsv.size();
        const std::string ln = tsv.substr(i, j - i);
        i = j + 1;
        if (ln.compare(0, 5, "w3da:") != 0) continue;
        const size_t tb = ln.find('\t');
        std::string f = ln.substr(5, tb == std::string::npos ? std::string::npos : tb - 5);
        for (size_t k = 0; k < f.size(); k++) if (f[k] == '_') f[k] = ' ';
        if (f.size() != 4) { err = "animsetregistro: fila w3da: rota: " + ln; return false; }
        if (ln.find("\tobsoleto\t") == std::string::npos) registrados.insert(f);
    }
    std::vector<std::string> conocidos, requeridos;
    W3dAnimSetBloquesConocidos(conocidos, &requeridos);
    int malos = 0;
    for (size_t k = 0; k < conocidos.size(); k++) {
        if (!registrados.count(conocidos[k])) { printf("      [animsetregistro] '%s' lo entiende el codigo y NO esta en bloques.tsv\n", conocidos[k].c_str()); malos++; }
        if (md.find("`" + conocidos[k] + "`") == std::string::npos) { printf("      [animsetregistro] '%s' no esta documentado en w3da.md\n", conocidos[k].c_str()); malos++; }
    }
    for (std::set<std::string>::const_iterator it = registrados.begin(); it != registrados.end(); ++it) {
        bool esta = false;
        for (size_t k = 0; k < conocidos.size(); k++) if (conocidos[k] == *it) esta = true;
        if (!esta) { printf("      [animsetregistro] '%s' esta registrado vivo y el codigo no lo entiende\n", it->c_str()); malos++; }
    }
    printf("      [animsetregistro] codigo=%d registrados=%d requeridos=%d -> %s\n", (int)conocidos.size(),
           (int)registrados.size(), (int)requeridos.size(), malos ? "MAL" : "OK");
    if (malos) { err = "animsetregistro: el codigo, formato/bloques.tsv y formato/w3da.md no coinciden"; return false; }
    return true;
}

// ============================================================================
//  SELLOS: los clips de un animset cargado son INMUTABLES. Antes de cada comando
//  (y con 'animsetsellos') se re-calcula la huella de CADA clip de cada animset vivo
//  (los canonicos y los de las variantes) y se compara con la que tenia la primera
//  vez que se lo vio: si cambio, algo escribio un clip compartido sin pasar por
//  W3dArmatureAnimsPropias (copy-on-write) y le cambio la animacion a TODOS los que
//  lo usan. Es la defensa de la suite contra un camino de edicion nuevo que se
//  olvide de copiar (el editor no tiene red por frame: copia solo el que escribe).
//
//  La clave del sello NO es el puntero del clip (un clip liberado y otro nuevo en la
//  misma direccion darian un falso positivo) sino (animset, huella del archivo del que
//  se leyo, variante, indice del clip): releer el mismo archivo da los mismos clips.
// ============================================================================
struct ClaveSello {
    const void* set; unsigned h1, h2; unsigned long tam; int var; unsigned mapa; int k;
    bool operator<(const ClaveSello& o) const {
        if (set != o.set) return set < o.set;
        if (h1 != o.h1) return h1 < o.h1;
        if (h2 != o.h2) return h2 < o.h2;
        if (tam != o.tam) return tam < o.tam;
        if (var != o.var) return var < o.var;
        if (mapa != o.mapa) return mapa < o.mapa;
        return k < o.k;
    }
};
std::map<ClaveSello, unsigned> gSellos;
bool gSellosOn = true;

// mezcla de 32 bits por PALABRA (no por byte): un nivel grande son ~1,7 M keyframes por verificacion
inline void Mezclar(unsigned& h, unsigned v) { h ^= v; h *= 16777619u; h ^= h >> 15; }
inline unsigned BitsF(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

unsigned HuellaClip(const SkeletalAnimation* c) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < c->name.size(); i++) Mezclar(h, (unsigned char)c->name[i]);
    Mezclar(h, (unsigned)c->FrameRate); Mezclar(h, (unsigned)c->startFrame); Mezclar(h, (unsigned)c->endFrame);
    Mezclar(h, (unsigned)c->tracks.size());
    for (size_t t = 0; t < c->tracks.size(); t++) {
        const BoneTrack& tr = c->tracks[t];
        Mezclar(h, (unsigned)tr.bone); Mezclar(h, (unsigned)tr.Propertys.size());
        for (size_t p = 0; p < tr.Propertys.size(); p++) {
            const AnimProperty& ap = tr.Propertys[p];
            Mezclar(h, (unsigned)ap.Property); Mezclar(h, (unsigned)ap.component); Mezclar(h, (unsigned)ap.keyframes.size());
            for (size_t k = 0; k < ap.keyframes.size(); k++) {
                const keyFrame& kf = ap.keyframes[k];
                Mezclar(h, (unsigned)kf.frame); Mezclar(h, BitsF(kf.value));
                Mezclar(h, (unsigned)kf.Interpolation); Mezclar(h, (unsigned)kf.handleType);
                Mezclar(h, BitsF(kf.inDF)); Mezclar(h, BitsF(kf.inDV)); Mezclar(h, BitsF(kf.outDF)); Mezclar(h, BitsF(kf.outDV));
            }
        }
    }
    return h;
}

// false = algun clip compartido cambio ('motivo' dice cual). 'nClips': cuantos se verificaron.
bool SellosVerificar(std::string* motivo, int* nClips) {
    std::map<ClaveSello, unsigned> vistos;
    std::vector<W3dRecurso*> todos;
    W3dRecursosListar(W3DREC_ANIMSET, todos);
    bool ok = true;
    int n = 0;
    for (size_t i = 0; i < todos.size(); i++) {
        const W3dRecurso* r = todos[i];
        if (!r || r->estado != W3DREC_LISTO || !r->dato) continue;
        const W3dAnimSet* s = (const W3dAnimSet*)r->dato;
        for (int v = -1; v < (int)s->variantes.size(); v++) {
            const std::vector<SkeletalAnimation*>& lista = (v < 0) ? s->datos.clips : s->variantes[v]->clips;
            unsigned hm = 2166136261u;
            if (v >= 0) for (size_t m = 0; m < s->variantes[v]->mapa.size(); m++) Mezclar(hm, (unsigned)s->variantes[v]->mapa[m]);
            for (size_t k = 0; k < lista.size(); k++) {
                if (!lista[k]) continue;   // vacio: se lo llevo su unico usuario (ya no es del recurso)
                ClaveSello c;
                c.set = s; c.h1 = s->huella1; c.h2 = s->huella2; c.tam = (unsigned long)s->tamArchivo;
                c.var = v; c.mapa = hm; c.k = (int)k;
                const unsigned h = HuellaClip(lista[k]);
                std::map<ClaveSello, unsigned>::const_iterator it = gSellos.find(c);
                if (it != gSellos.end() && it->second != h && ok) {
                    ok = false;
                    if (motivo) {
                        char b[96]; sprintf(b, "' (clip %d%s) ", (int)k, v >= 0 ? " de una variante" : "");
                        *motivo = "el clip COMPARTIDO '" + lista[k]->name + b + "del animset '" + s->nombre +
                                  "' cambio en memoria: algo lo escribio sin copy-on-write (W3dArmatureAnimsPropias)";
                    }
                }
                vistos[c] = (it != gSellos.end() && it->second != h) ? it->second : h;
                n++;
            }
        }
    }
    // lo que ya no esta (animsets liberados, casilleros vaciados) se olvida
    gSellos.swap(vistos);
    if (nClips) *nClips = n;
    return ok;
}

// el DETECTOR, probado: escribe un keyframe de un clip compartido SIN copiar (lo que haria un camino
// de edicion que se olvide), verifica que los sellos lo vean, y lo deja como estaba
bool SellosPrueba(const std::string& n, std::string& err) {
    Armature* a = ArmPorNombre(n);
    if (!a || !W3dArmatureAnimsCompartidas(a)) { err = "animsetsellos prueba: '" + n + "' no tiene clips compartidos"; return false; }
    std::string motivo;
    if (!SellosVerificar(&motivo, 0)) { err = "animsetsellos prueba: ya estaban rotos: " + motivo; return false; }
    keyFrame* k = 0;
    for (size_t c = 0; c < a->animations.size() && !k; c++) {
        SkeletalAnimation* clip = a->animations[c];
        for (size_t t = 0; clip && t < clip->tracks.size() && !k; t++)
            for (size_t p = 0; p < clip->tracks[t].Propertys.size() && !k; p++)
                if (!clip->tracks[t].Propertys[p].keyframes.empty()) k = &clip->tracks[t].Propertys[p].keyframes[0];
    }
    if (!k) { err = "animsetsellos prueba: sin keyframes"; return false; }
    const float v0 = k->value;
    k->value = v0 + 1.0f;                       // la escritura PROHIBIDA
    const bool visto = !SellosVerificar(&motivo, 0);
    k->value = v0;                              // como estaba
    const bool sano = SellosVerificar(0, 0);
    printf("      [animsetsellos] prueba en '%s': escribir sin copiar %s; restaurado %s\n", n.c_str(),
           visto ? ("SE DETECTA (" + motivo + ")").c_str() : "NO SE DETECTA", sano ? "INTACTO" : "ROTO");
    if (!visto) { err = "animsetsellos prueba: una escritura en un clip compartido NO se detecto"; return false; }
    if (!sano)  { err = "animsetsellos prueba: restaurado el valor, los sellos siguen rotos"; return false; }
    return true;
}

// ---- animsetsellos [off|on] [clips N] | animsetsellos prueba <armature> ----
bool CmdSellos(std::istringstream& ss, std::string& err) {
    std::string k;
    long quiere = -1;
    while (ss >> k) {
        if (k == "prueba") return SellosPrueba(LeerPalabra(ss), err);
        if (k == "off") { gSellosOn = false; printf("      [animsetsellos] APAGADOS (bench)\n"); return true; }
        if (k == "on")  { gSellosOn = true; }
        else if (k == "clips") { ss >> quiere; }
        else { err = "animsetsellos: uso: animsetsellos [off|on] [clips N]"; return false; }
    }
    std::string motivo; int n = 0;
    const bool ok = SellosVerificar(&motivo, &n);
    printf("      [animsetsellos] %d clips compartidos verificados: %s\n", n, ok ? "INTACTOS" : "ALGUNO CAMBIO");
    if (!ok) { err = "animsetsellos: " + motivo; return false; }
    if (quiere >= 0 && quiere != n) { char b[96]; sprintf(b, "animsetsellos: clips = %d, se esperaba %ld", n, quiere); err = b; return false; }
    return true;
}

// ============================================================================
//  animsetver / animsetdope: el armature como lo deja el USUARIO para mirar o editar su
//  clip (objeto activo en Pose Mode, todos los huesos elegidos, el clip en el timeline).
//  'animsetver' hace todo lo que es MIRAR (dope sheet y editor de curvas: filas, dibujo,
//  encuadre, click en un keyframe, la tarjeta Keyframe, reproducir, dibujar la UI entera)
//  y asierta que el armature SIGUE compartiendo sus clips. 'animsetdope' hace UNA edicion
//  por el camino real de la UI y asierta que el armature paso a clips propios; los sellos
//  del proximo comando prueban que el animset (y los demas usuarios) no se entero.
// ============================================================================
void PrepararTimeline(Armature* a, int clip) {
    DeseleccionarTodo();
    a->select = true;
    ObjActivo = a;
    estado = editNavegacion;
    InteractionMode = PoseMode;
    for (size_t i = 0; i < a->bones.size(); i++) a->bones[i].select = true;
    a->boneActivo = a->bones.empty() ? -1 : 0;
    ActiveAnimKind = 1; ActiveAnimArm = a;
    a->animActiva = clip;
    AnimCargarRangoActivo();
    a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = true;
}

// y al terminar, de vuelta a Object Mode con la animacion de escena (los comandos siguientes no
// heredan el Pose Mode ni el clip en el timeline)
void SoltarTimeline() {
    InteractionMode = ObjectMode;
    estado = editNavegacion;
    ActiveAnimKind = 0; ActiveAnimArm = 0;
    AnimCargarRangoActivo();
}

// la primera fila de CANAL con al menos 'minKeys' keyframes (NULL = no hay)
Timeline::DopeRow* FilaCanal(Timeline* tl, size_t minKeys) {
    for (size_t i = 0; i < tl->dopeRows.size(); i++)
        if (tl->dopeRows[i].propId >= 0 && tl->dopeRows[i].keys.size() >= minKeys) return &tl->dopeRows[i];
    return 0;
}

// click REAL (CurvaClickStrip) sobre el keyframe 'i' de la fila: queda elegido y ACTIVO (tarjeta Keyframe)
bool ClickKeyframe(Timeline* tl, const Timeline::DopeRow& d, size_t i) {
    const AnimProperty* c = tl->CurvaDeFila(d, false);   // solo para ubicar el click
    if (!c || i >= c->keyframes.size()) return false;
    const keyFrame& k = c->keyframes[i];
    tl->viewCenterV = k.value;
    tl->DopeSelectNone();
    return tl->CurvaClickStrip((int)(tl->x + tl->FrameToX((float)k.frame)), (int)(tl->y + tl->ValueToY(k.value)));
}

ViewportBase* BuscarHoja(ViewportBase* n, int kind) {
    if (!n) return 0;
    if (n->isLeaf()) return n->ViewportKind() == kind ? n : 0;
    ViewportBase* a = BuscarHoja(((ViewportColumn*)n)->childA, kind);
    return a ? a : BuscarHoja(((ViewportColumn*)n)->childB, kind);
}

bool AssertCompartido(const char* cmd, Armature* a, std::istringstream& ss, int copias0, std::string& err) {
    std::vector<std::string> k, v;
    if (!LeerAsserts(ss, k, v, cmd, err)) return false;
    for (size_t i = 0; i < k.size(); i++) {
        long tiene = 0;
        if      (k[i] == "compartido") tiene = W3dArmatureAnimsCompartidas(a) ? 1 : 0;
        else if (k[i] == "copias")     tiene = W3dAnimSetsCopias() - copias0;   // las de ESTE comando
        else { err = std::string(cmd) + ": assert desconocido '" + k[i] + "'"; return false; }
        if (!AssertInt(cmd, k[i], v[i], tiene, err)) return false;
    }
    return true;
}

// ---- animsetver <armature> <clip> [compartido 0|1] [copias N] ----
bool CmdVer(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    int clip = 0; ss >> clip;
    Armature* a = ArmPorNombre(n);
    if (!a || clip < 0 || clip >= (int)a->animations.size()) { err = "animsetver: uso: animsetver <armature> <clip> [asserts]"; return false; }
    const int copias0 = W3dAnimSetsCopias();
    PrepararTimeline(a, clip);
    // 1) DOPE SHEET: filas + seleccionar todo + rango de la seleccion
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->ConstruirDopeRows();
    const int filas = (int)tl->dopeRows.size();
    tl->DopeSelectAll();
    int mn = 0, mx = 0; tl->DopeRangoSeleccion(mn, mx);
    // 2) EDITOR DE CURVAS: encuadre, costo del trazo, click en un keyframe (hit-test), la tarjeta
    tl->modo = Timeline::TL_MODO_CURVAS;
    tl->ConstruirDopeRows();
    float f0, f1, v0, v1;
    tl->CurvaRangoVista(false, f0, f1, v0, v1);
    tl->CurvaRangoVista(true, f0, f1, v0, v1);
    const long long costo = tl->CurvaTrazoCosto();
    Timeline::DopeRow* canal = FilaCanal(tl, 3);
    const bool clic = canal && ClickKeyframe(tl, *canal, 1);
    int ki = -1;
    const AnimProperty* kap = DopeKeyframeActivo(&ki, false);
    // la tarjeta "Keyframe" del panel (pestania Animacion): ActualizarPestanias la refresca
    std::string e2;
    if (!W3dRunCommand("propstab 9", e2)) printf("      [animsetver] (sin panel de propiedades: %s)\n", e2.c_str());
    // 3) la UI ENTERA dibujada una vez, con el timeline del layout en modo curvas (RenderCurvas,
    //    el panel del dope, las propiedades, el 3D con el esqueleto posado)
    Timeline* tlLayout = (Timeline*)BuscarHoja(rootViewport, 5);
    const int modo0 = tlLayout ? tlLayout->modo : 0;
    if (tlLayout) { tlLayout->modo = Timeline::TL_MODO_CURVAS; tlLayout->ConstruirDopeRows(); }
    CargarTexturasPendientes();
    if (rootViewport) rootViewport->Render();
    if (tlLayout) tlLayout->modo = modo0;
    // 4) REPRODUCIR: la pose en varios frames del clip
    const SkeletalAnimation* c = a->animations[clip];
    for (int f = c->startFrame; f <= c->endFrame; f += 3) {
        a->lastPoseFrame = -999999; a->poseDirty = false;
        EvaluarPoseEsqueleto(a, f);
    }
    tl->DopeSelectNone();
    delete tl;
    SoltarTimeline();
    printf("      [animsetver] '%s' clip %d: %d filas, keys %d..%d, trazo=%lld vertices, click=%s, tarjeta=%s -> clips %s, copias=%d\n",
           a->name.c_str(), clip, filas, mn, mx, costo, clic ? "SI" : "no", kap ? "SI" : "no",
           W3dArmatureAnimsCompartidas(a) ? "COMPARTIDOS" : "PROPIOS", W3dAnimSetsCopias() - copias0);
    if (filas == 0 || !clic || !kap) { err = "animsetver: no se pudo mirar el clip (sin filas / sin keyframe activo)"; return false; }
    return AssertCompartido("animsetver", a, ss, copias0, err);
}

// ---- animsetdope <armature> <clip> <op> [compartido 0|1] [copias N] ----
//  op: mover | escalar | cancelar | borrar | duplicar | interp | handletipo | euler | kfvalor |
//      handle | insertar | fin | finigual | renombrar | renombrararm | undo
bool CmdDope(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    int clip = 0; ss >> clip;
    std::string op; ss >> op;
    Armature* a = ArmPorNombre(n);
    if (!a || clip < 0 || clip >= (int)a->animations.size() || op.empty()) { err = "animsetdope: uso: animsetdope <armature> <clip> <op> [asserts]"; return false; }
    const int copias0 = W3dAnimSetsCopias();
    PrepararTimeline(a, clip);
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->modo = Timeline::TL_MODO_CURVAS;
    tl->ConstruirDopeRows();
    Timeline::DopeRow* canal = FilaCanal(tl, 3);
    if (!canal) { delete tl; err = "animsetdope: el clip no tiene una curva con 3 keyframes"; return false; }
    const Timeline::DopeRow fila = *canal;
    bool hecho = true;
    if (op == "mover" || op == "escalar" || op == "cancelar") {
        tl->DopeSelectAll();
        tl->DopeMoveStart(op == "escalar" ? Timeline::DOPE_ESC : Timeline::DOPE_MOV);
        const char* num = (op == "escalar") ? "2" : "3";
        for (const char* q = num; *q; ++q) tl->DopeNumChar(*q);
        if (op == "cancelar") tl->DopeMoveCancel(); else tl->DopeMoveConfirm();
    } else if (op == "borrar") {
        hecho = ClickKeyframe(tl, fila, 1);
        tl->DopeBorrarSeleccion();
    } else if (op == "duplicar") {
        hecho = ClickKeyframe(tl, fila, 1);
        tl->DopeDuplicarSeleccion();
        for (const char* q = "7"; *q; ++q) tl->DopeNumChar(*q);
        tl->DopeMoveConfirm();
    } else if (op == "interp") {
        tl->DopeSelectAll();
        tl->SetInterpolacionSel(KfConstant);
    } else if (op == "handletipo") {
        tl->DopeSelectAll();
        tl->SetHandleTypeSel(HVector);
    } else if (op == "euler") {
        tl->DopeSelectAll();
        tl->SmartEulerSel();
    } else if (op == "kfvalor") {
        // lo que hacen las acciones de la tarjeta Keyframe (Properties.cpp): la curva EDITABLE
        hecho = ClickKeyframe(tl, fila, 1);
        int i = -1; AnimProperty* ap = hecho ? DopeKeyframeActivo(&i) : 0;
        if (ap) { UndoKeyframesIniciar(); ap->keyframes[i].value += 0.5f; UndoKeyframesConfirmar(); }
        hecho = hecho && ap;
    } else if (op == "handle") {
        // agarrar el handle de SALIDA de un keyframe BEZIER (el tramo que sale se dibuja curvo, y el
        // handle del keyframe ACTIVO se ve y se agarra) y arrastrarlo: curva el tramo
        hecho = false;
        for (size_t r = 0; r < tl->dopeRows.size() && !hecho; r++) {
            const Timeline::DopeRow d = tl->dopeRows[r];
            const AnimProperty* ap = tl->CurvaDeFila(d, false);
            if (!ap) continue;
            size_t k = 0;
            while (k + 1 < ap->keyframes.size() && ap->keyframes[k].Interpolation != KfBezier) k++;
            if (k + 1 >= ap->keyframes.size() || !ClickKeyframe(tl, d, k)) continue;
            // el click elige el keyframe MAS CERCANO (puede ser el de otra curva en el mismo punto): el
            // handle es el del keyframe que quedo ACTIVO
            int ki = -1; const AnimProperty* act = DopeKeyframeActivo(&ki, false);
            if (!act || ki < 0 || act->keyframes[ki].Interpolation != KfBezier) continue;
            float hx = 0, hy = 0;
            tl->HandlePos(act, (size_t)ki, true, hx, hy);
            if (!tl->CurvaClickStrip((int)(tl->x + hx), (int)(tl->y + hy)) || !tl->HandleArrastrando()) continue;
            for (int q = 1; q <= 3; q++) tl->HandleApply((int)(tl->x + hx) + 12 * q, (int)(tl->y + hy) - 15 * q);
            tl->HandleSoltar();
            hecho = true;
        }
    } else if (op == "insertar") {
        // Insert Keyframe de Pose Mode (la I) en el SEGUNDO frame del clip (uno de los de posefoto):
        // la pose de ahi con la raiz girada 5 grados
        CurrentFrame = a->animations[clip]->startFrame + 1;
        a->lastPoseFrame = -999999; a->poseDirty = false;
        EvaluarPoseEsqueleto(a, CurrentFrame);
        a->bones[0].poseR = a->bones[0].poseR + Vector3(5.0f, 0.0f, 0.0f);
        InsertarKeyframeEsqueleto(a, 0);
    } else if (op == "renombrar" || op == "renombrararm") {
        // los DOS botones Rename del clip, por el camino real: el boton arranca el rename in-place
        // (toma el PUNTERO al nombre), se tipea y Enter. 'renombrar' = tarjeta Animacion (el clip del
        // timeline); 'renombrararm' = tarjeta del armature activo
        std::string e2;
        W3dRunCommand("propstab 9", e2);   // PropsActivo (el panel del layout)
        PropButton* b = !PropsActivo ? 0 : (op == "renombrar") ? PropsActivo->propBtnAnimRename : PropsActivo->propBtnRenameAnim;
        hecho = false;
        if (b && b->action) {
            b->action();
            if (g_textFieldActivo) {
                g_textFieldActivo->SetText("Renombrado");
                RenameCommit();
                hecho = (a->animations[clip]->name == "Renombrado");
            }
        }
    } else if (op == "fin" || op == "finigual") {
        // el End del clip (campo del timeline / tarjeta Animacion: AnimSetEnd). Copia SOLO si el
        // valor cambia: confirmar el campo sin tocarlo no le cuesta memoria a nadie
        const int fin = a->animations[clip]->endFrame;
        AnimSetEnd(op == "fin" ? fin + 5 : fin);
    } else if (op == "undo") {
        UndoDeshacer();
    } else { delete tl; err = "animsetdope: op desconocida '" + op + "'"; return false; }
    InvalidarAnimYRedraw();
    tl->DopeSelectNone();
    delete tl;
    SoltarTimeline();
    printf("      [animsetdope] '%s' clip %d %s: %s -> clips %s, copias=%d\n", a->name.c_str(), clip, op.c_str(),
           hecho ? "hecho" : "NO SE PUDO", W3dArmatureAnimsCompartidas(a) ? "COMPARTIDOS" : "PROPIOS",
           W3dAnimSetsCopias() - copias0);
    if (!hecho) { err = "animsetdope: la operacion '" + op + "' no se pudo hacer"; return false; }
    return AssertCompartido("animsetdope", a, ss, copias0, err);
}

// ============================================================================
//  LOS ANIMSETS COMO RECURSOS DEL PROYECTO (io/GuardarAnimSets.h: lo que usa el
//  outliner por recursos)
//    animsetusuarios <animset> <N>          cuantos armatures lo usan
//    animsetrenombrarset <viejo> <nuevo> [<esperado>]
//    animsetcarpeta <animset> fijar|es <carpeta>
//    animsetpurgar [N]                      saca del registro los huerfanos
// ============================================================================
bool CmdUsuarios(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    long quiere = -1; ss >> quiere;
    std::vector<Armature*> us;
    const int k = W3dAnimSetUsuarios(SceneCollection, n, &us);
    std::string lista;
    for (size_t i = 0; i < us.size(); i++) lista += (i ? ", " : "") + us[i]->name;
    printf("      [animsetusuarios] '%s': %d (%s)\n", n.c_str(), k, lista.c_str());
    if (quiere >= 0 && quiere != k) { char b[128]; sprintf(b, "animsetusuarios: %d usuarios, se esperaban %ld", k, quiere); err = b; return false; }
    return true;
}
bool CmdRenombrarSet(std::istringstream& ss, std::string& err) {
    const std::string viejo = LeerPalabra(ss), nuevo = LeerPalabra(ss), esperado = LeerPalabra(ss);
    std::string final;
    if (!W3dAnimSetRenombrar(SceneCollection, viejo, nuevo, &final)) { err = "animsetrenombrarset: no hay un animset '" + viejo + "'"; return false; }
    printf("      [animsetrenombrarset] '%s' -> '%s'\n", viejo.c_str(), final.c_str());
    if (!esperado.empty() && final != esperado) { err = "animsetrenombrarset: quedo '" + final + "', se esperaba '" + esperado + "'"; return false; }
    return true;
}
bool CmdCarpeta(std::istringstream& ss, std::string& err) {
    const std::string n = LeerPalabra(ss);
    std::string modo; ss >> modo;
    const std::string carpeta = LeerPalabra(ss);
    if (modo == "fijar") {
        if (!W3dAnimSetFijarCarpeta(n, carpeta)) { err = "animsetcarpeta: no hay un animset '" + n + "'"; return false; }
        printf("      [animsetcarpeta] '%s' -> carpeta '%s'\n", n.c_str(), carpeta.c_str());
        return true;
    }
    if (modo != "es") { err = "animsetcarpeta: uso: animsetcarpeta <animset> fijar|es <carpeta>"; return false; }
    const std::vector<W3dAnimSetFila>& reg = W3dAnimSetsRegistro();
    for (size_t i = 0; i < reg.size(); i++) {
        if (reg[i].nombre != n) continue;
        printf("      [animsetcarpeta] '%s' esta en la carpeta '%s'\n", n.c_str(), reg[i].carpeta.c_str());
        if (reg[i].carpeta != carpeta) { err = "animsetcarpeta: carpeta = '" + reg[i].carpeta + "', se esperaba '" + carpeta + "'"; return false; }
        return true;
    }
    err = "animsetcarpeta: no hay un animset '" + n + "' en el registro";
    return false;
}
bool CmdPurgar(std::istringstream& ss, std::string& err) {
    long quiere = -1; ss >> quiere;
    std::vector<std::string> fuera;
    const int k = W3dAnimSetsPurgarHuerfanos(SceneCollection, &fuera);
    std::string lista;
    for (size_t i = 0; i < fuera.size(); i++) lista += (i ? ", " : "") + fuera[i];
    printf("      [animsetpurgar] %d huerfanos fuera del registro (%s)\n", k, lista.c_str());
    if (quiere >= 0 && quiere != k) { char b[128]; sprintf(b, "animsetpurgar: %d, se esperaban %ld", k, quiere); err = b; return false; }
    return true;
}

} // namespace

// la linea de los animsets de 'meminfo' (W3dScript.cpp la llama; el resto del informe es de alla)
void W3dPruebasAnimsMeminfo() {
    W3dAnimSetsStats st;
    W3dAnimSetsEstadisticas(st);
    // lo que ocuparian los clips de los armatures COLGADOS si cada uno tuviera los suyos
    std::vector<Armature*> arms;
    JuntarArms(SceneCollection, arms);
    long sin = 0;
    for (size_t i = 0; i < arms.size(); i++) {
        if (!W3dArmatureAnimsCompartidas(arms[i])) continue;
        for (size_t c = 0; c < arms[i]->animations.size(); c++) sin += W3dAnimSetClipBytes(arms[i]->animations[c], 0);
    }
    const long ahorro = sin > st.bytes ? sin - st.bytes : 0;
    printf("      [meminfo] animsets: vivos=%d refs=%d clips=%d (+%d de variantes) keys=%ld compartido=%ld B (%.1f KB) AHORRO=%ld B (%.1f KB) copias COW=%d\n",
           st.vivos, st.refs, st.clips, st.clipsVariante, st.keys, st.bytes, st.bytes / 1024.0, ahorro, ahorro / 1024.0,
           W3dAnimSetsCopias());
}

bool W3dPruebasAnimsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    // SELLOS: ningun clip de un animset cargado puede haber cambiado desde el comando anterior
    // (el editor copia solo al escribir: un camino que escriba sin copiar se ve aca, no en la
    // animacion de otro personaje). 'animsetsellos off' lo apaga para medir.
    if (gSellosOn && cmd != "animsetsellos") {
        std::string motivo;
        if (!SellosVerificar(&motivo, 0)) { err = "animsets (antes de este comando): " + motivo; return false; }
    }
    // los comandos de W3dScript.cpp que escriben el clip del armature ACTIVO sin pasar por el Core
    // (atajos del harness, no caminos del editor): copian antes, como lo haria el editor. Los demas
    // 'anim' (add/dup/del/up/down) van por el Core / el undo, que copian solos (o no escriben clips)
    if ((cmd == "anim" || cmd == "animkey") && ObjActivo && ObjActivo->getType() == ObjectType::armature) {
        std::istringstream s2(ss.str());
        std::string c0, sub;
        s2 >> c0 >> sub;
        if (cmd == "animkey" || sub == "rename") W3dArmatureAnimsPropias((Armature*)ObjActivo);
    }
    if (cmd == "animsetinfo")      return CmdInfo(ss, err);
    if (cmd == "animsetarm")       return CmdArm(ss, err);
    if (cmd == "animsetrec")       return CmdRec(ss, err);
    if (cmd == "animsetclip")      return CmdClip(ss, err);
    if (cmd == "animsetsw3d")      return CmdSw3d(ss, err);
    if (cmd == "animsinline")      { int v = 0; ss >> v; g_w3dAnimsInline = (v != 0);
                                     printf("      [animsinline] %s\n", g_w3dAnimsInline ? "los clips se guardan INLINE (formato de antes)" : "animsets .w3da");
                                     return true; }
    if (cmd == "animsetsintetica") return CmdSintetica(ss, err);
    if (cmd == "animsetapuntar")   return CmdApuntar(ss, err);
    if (cmd == "animsetrenombrar") return CmdRenombrar(ss, err);
    if (cmd == "animsetborrar")    return CmdBorrar(ss, err);
    if (cmd == "animsetactivo")    return CmdActivo(ss, err);
    if (cmd == "animsetluajuego")  return CmdLuaJuego(ss, err);
    if (cmd == "animsetjuegoquitar") return CmdJuegoQuitar(ss, err);
    if (cmd == "animsetpistas")    return CmdPistas(ss, err);
    if (cmd == "animsetborrarhueso") return CmdBorrarHueso(ss, err);
    if (cmd == "animsetrenombrarhueso") return CmdRenombrarHueso(ss, err);
    if (cmd == "posefoto")         return CmdPoseFoto(ss, err);
    if (cmd == "posefotoigual")    return CmdPoseFotoIgual(ss, err);
    if (cmd == "animsetromper")    return CmdRomper(ss, err);
    if (cmd == "animsetcorrupto")  return CmdCorrupto(err);
    if (cmd == "animsetregistro")  return CmdRegistro(err);
    if (cmd == "animsetsellos")    return CmdSellos(ss, err);
    if (cmd == "animsetver")       return CmdVer(ss, err);
    if (cmd == "animsetdope")      return CmdDope(ss, err);
    if (cmd == "animsetusuarios")  return CmdUsuarios(ss, err);
    if (cmd == "animsetrenombrarset") return CmdRenombrarSet(ss, err);
    if (cmd == "animsetcarpeta")   return CmdCarpeta(ss, err);
    if (cmd == "animsetpurgar")    return CmdPurgar(ss, err);
    manejado = false;
    return false;
}
