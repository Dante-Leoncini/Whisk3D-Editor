// ===================================================================================================
//  EDIT MODE de HUESOS (Fase 3). Ver BoneEdit.h. Comparte las operaciones entre el viewport 3D, el
//  UV editor (armature 2D = mismos huesos con Z=0) y el harness headless (bonetest).
//  Convenciones:
//   - head/tail viven en el espacio LOCAL del armature (para rigs autorados coincide con el espacio
//     del FK: PrepararSkinAutorado deriva el rest de ahi).
//   - "conectado" es IMPLICITO: head del hijo == tail del padre (el dibujo pone la linea punteada
//     cuando hay gap). Extruir crea conectado; duplicar NO conecta; borrar re-parenta los hijos.
//   - SELECCION estilo Blender: hueso ENTERO (select) o PUNTAS sueltas (selHead/selTail). La punta
//     COMPARTIDA de un par conectado se selecciona y se mueve como UNA sola (soldada): tocarla
//     prende el tail del padre Y el head de cada hijo conectado a la vez.
//   - cada operacion discreta empuja su paso de undo (UndoBonesCapturar, snapshot de bones+tracks).
//   - el TRANSFORM MODAL (G/R/S) del 3D usa el estado compartido del editor (estado/axisSelect/
//     transformOrientation + NumInput) igual que Pose Mode -> barra de estado, linea de eje X/Y/Z
//     y valor numerico exacto funcionan sin codigo extra en el viewport.
// ===================================================================================================
#include "edit/BoneEdit.h"
#include "W3dNombres.h"                  // LA regla de nombres unicos (compartida por todo el editor)
#include "Undo.h"
#include "variables.h"                    // InteractionMode / EditMode / ObjActivo / lastMouse / TransformPivotPoint / cursor3D
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Armature.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h"                // ModifierType::Armature (mallas ligadas al rig, para el rename)
#include "animation/SkeletalAnimation.h"  // PrepararSkinAutorado + BoneTrack (remap al borrar)
#include "ViewPorts/ViewPort3D.h"         // ProyectarPunto / VelocidadArrastreMundo (grab 3D)
#include "ViewPorts/Notificaciones.h"     // aviso al renombrar el vertex group junto al hueso
#include "ViewPorts/Timeline.h"           // DopeRemapIndiceClave: borrar un hueso CORRE los indices que
                                          // guarda la seleccion del dope sheet ("arm:<rig>/b<IDX>")
#include "W3dAviso.h"                     // ...armado SIN desbordar (los nombres no tienen cota)
#include "render/OpcionesRender.h"        // g_redraw + g_transformPivot (selector de pivote)
#include "math/Quaternion.h"
#include <math.h>
#include <stdio.h>

extern void InvalidarSkinDeArmature(Armature* a); // ObjectMode.cpp
extern void NumInputReset();

bool BoneEditActivo(){
    return InteractionMode == EditMode && ObjActivo && ObjActivo->getType() == ObjectType::armature;
}
Armature* BoneEditArm(){ return BoneEditActivo() ? (Armature*)ObjActivo : NULL; }

// post-edicion comun: el rest de un rig autorado sale de head/tail -> re-preparar el skin con lo nuevo.
// Un armature MANUAL sin preparar se prepara aca mismo (la guarda interna no toca rigs importados).
void BoneEditAplicado(Armature* a){
    if (!a) return;
    PrepararSkinAutorado(a); // no-op en rigs importados (algun hueso con rest de FBX/glTF)
    a->lastPoseFrame = -999999; a->poseSerial++;
    InvalidarSkinDeArmature(a);
    g_redraw = true;
}

// nombre UNICO dentro del armature ("Bone", "Bone.001", ...). 'excepto' = indice que puede conservarlo (rename).
// Delega en LA regla comun (base/W3dNombres.h): antes tenia su propio bucle con tope en
// 999 que devolvia "Bone.x" (un nombre que tambien podia estar ocupado).
struct BoneNomCtx { Armature* a; int excepto; };
static bool BoneNombreExisteCb(const std::string& n, void* ctx){
    BoneNomCtx* c = (BoneNomCtx*)ctx;
    for (size_t i = 0; i < c->a->bones.size(); i++)
        if ((int)i != c->excepto && c->a->bones[i].name == n) return true;
    return false;
}
std::string BoneNombreLibre(Armature* a, const std::string& base, int excepto){
    if (!a) return base;
    BoneNomCtx c; c.a = a; c.excepto = excepto;
    return W3dNombreUnico(base, "Bone", BoneNombreExisteCb, &c);
}
static std::string BoneNombreUnico(Armature* a, const std::string& base, int excepto){
    return BoneNombreLibre(a, base, excepto);
}

// ---------------------------------------------------------------------------------------------------
//  SELECCION por puntas/huesos
// ---------------------------------------------------------------------------------------------------
// conectado = head del hijo pegado al tail del padre Y el flag 'conectado' prendido ("Disconnect
// Bone" lo apaga sin mover nada: la punta deja de estar soldada aunque siga coincidiendo). Los
// rigs autorados extruyen EXACTO (misma posicion); el epsilon tolera redondeos de guardado/carga.
//
// SOLO la geometria (sin mirar el flag): aca vive LA tolerancia, y el chequeo del flag y el saneador
// del invariante la comparten para no poder discrepar entre si.
static bool BonePuntasSoldadas(Armature* a, int b){
    if (!a || b < 0 || b >= (int)a->bones.size()) return false;
    const int p = a->bones[b].parent;
    if (p < 0 || p >= (int)a->bones.size()) return false;
    return (a->bones[b].head - a->bones[p].tail).LengthSq() < 1e-8f;
}

bool BoneEsConectado(Armature* a, int b){
    if (!a || b < 0 || b >= (int)a->bones.size()) return false;
    if (!a->bones[b].conectado) return false;
    return BonePuntasSoldadas(a, b);
}

// INVARIANTE del flag: 'conectado' puede estar en true SOLO si el hueso tiene padre y su head cae en el
// tail de ese padre. Toda op que RE-PARENTE sin mover el hueso (borrar el hueso del medio -> los hijos
// pasan al abuelo, Clear Parent, duplicar, el desplegable Parent, Ctrl+P) puede dejar el flag prendido con
// las puntas separadas: el hueso "dice" que esta soldado y no lo esta. Esto lo restaura de una pasada.
// NO MUEVE NADA: solo apaga el flag donde miente (el hueso que si coincide lo conserva).
// Se llama al final de esas ops para que el invariante no dependa de acordarse en cada una.
bool BoneEditSanearConectado(Armature* a){
    if (!a) return false;
    bool cambio = false;
    for (size_t i = 0; i < a->bones.size(); i++){
        if (!a->bones[i].conectado) continue;
        if (BonePuntasSoldadas(a, (int)i)) continue;   // sigue soldado de verdad -> queda
        a->bones[i].conectado = false;
        cambio = true;
    }
    return cambio;
}

void BoneSelLimpiar(Armature* a){
    if (!a) return;
    for (size_t i = 0; i < a->bones.size(); i++){
        a->bones[i].select  = false;
        a->bones[i].selHead = false;
        a->bones[i].selTail = false;
    }
}

void BoneSelHueso(Armature* a, int b, bool sel){
    if (!a || b < 0 || b >= (int)a->bones.size()) return;
    a->bones[b].select  = sel;
    a->bones[b].selHead = sel;
    a->bones[b].selTail = sel;
}
// prende/apaga el flag crudo de la punta (sin sincronizar la soldada)
static void SelPuntaRaw(Armature* a, int b, int punta, bool sel){
    if (b < 0 || b >= (int)a->bones.size()) return;
    if (punta == 1) a->bones[b].selHead = sel; else a->bones[b].selTail = sel;
    // punta apagada a mano: el hueso ya no esta "entero" seleccionado
    if (!sel) a->bones[b].select = false;
}
// selecciona la punta Y todas sus soldadas: el tail del padre + el head de CADA hijo conectado a ese
// mismo punto (dos hermanos conectados comparten la punta a traves del tail del padre).
void BoneSelPunta(Armature* a, int b, int punta, bool sel){
    if (!a || b < 0 || b >= (int)a->bones.size()) return;
    SelPuntaRaw(a, b, punta, sel);
    int dueTail = -1; // el hueso cuyo TAIL es esta punta compartida (si existe)
    if (punta == 2) dueTail = b;
    else if (BoneEsConectado(a, b)) dueTail = a->bones[b].parent;
    if (dueTail < 0) return;                  // head raiz sin conectar: punta propia, nada mas
    SelPuntaRaw(a, dueTail, 2, sel);
    for (size_t c = 0; c < a->bones.size(); c++)
        if (a->bones[c].parent == dueTail && BoneEsConectado(a, (int)c))
            SelPuntaRaw(a, (int)c, 1, sel);
}
// hay alguna punta o hueso seleccionado?
static bool BoneHaySeleccion(Armature* a){
    if (!a) return false;
    for (size_t i = 0; i < a->bones.size(); i++)
        if (a->bones[i].select || a->bones[i].selHead || a->bones[i].selTail) return true;
    return false;
}

// arma la lista bones/masks del transform desde la SELECCION (hueso entero = mask 3; puntas = 1/2).
// Sin seleccion cae al hueso activo entero. Luego SUELDA: todo tail que se mueve arrastra el head de
// sus hijos conectados (asi el par sigue pegado; es la misma regla one-way que Blender).
static bool BoneSeleccionMasks(Armature* a, std::vector<int>& bones, std::vector<int>& masks){
    bones.clear(); masks.clear();
    if (!a) return false;
    int n = (int)a->bones.size();
    std::vector<int> mask(n, 0);
    for (int i = 0; i < n; i++){
        W3dBone& b = a->bones[i];
        if (b.select) mask[i] = 3;
        else mask[i] = (b.selHead ? 1 : 0) | (b.selTail ? 2 : 0);
    }
    bool hay = false;
    for (int i = 0; i < n; i++) if (mask[i]) { hay = true; break; }
    if (!hay){
        if (a->boneActivo >= 0 && a->boneActivo < n) mask[a->boneActivo] = 3;
        else return false;
    }
    // soldadura: tail en movimiento -> head del hijo conectado tambien (iterar hasta estabilizar:
    // un head arrastrado no arrastra nada mas, pero el or puede encadenar con huesos enteros)
    bool cambio = true;
    int guard = 0;
    while (cambio && guard++ <= n){
        cambio = false;
        for (int c = 0; c < n; c++){
            int p = a->bones[c].parent;
            if (p < 0 || p >= n || !(mask[p] & 2)) continue;
            if (!BoneEsConectado(a, c)) continue;
            if (!(mask[c] & 1)){ mask[c] |= 1; cambio = true; }
        }
    }
    for (int i = 0; i < n; i++)
        if (mask[i]){ bones.push_back(i); masks.push_back(mask[i]); }
    return !bones.empty();
}

// ---------------------------------------------------------------------------------------------------
//  operaciones discretas
// ---------------------------------------------------------------------------------------------------

// una PUNTA de origen para extruir: hueso + extremo (1=head 2=tail)
struct BonePunta { int b; int punta; };
// junta las puntas seleccionadas SIN repetir la compartida: la punta canonica de un head conectado
// es el TAIL de su padre. Hueso entero seleccionado = su tail (criterio Blender).
static void BonePuntasSeleccionadas(Armature* a, std::vector<BonePunta>& out){
    out.clear();
    int n = (int)a->bones.size();
    std::vector<char> tailUsada(n, 0);   // puntas-tail ya anotadas (dedupe de la compartida)
    std::vector<BonePunta> heads;        // heads raiz (puntas propias), van despues de los tails
    for (int i = 0; i < n; i++){
        W3dBone& b = a->bones[i];
        bool tail = b.select || b.selTail;              // entero -> tail
        bool head = !b.select && b.selHead;             // head suelto seleccionado
        if (head){
            if (BoneEsConectado(a, i)){                 // punta compartida -> canonica: tail del padre
                int p = b.parent;
                if (p >= 0 && p < n && !tailUsada[p]){ tailUsada[p] = 1; BonePunta e; e.b = p; e.punta = 2; out.push_back(e); }
            } else { BonePunta e; e.b = i; e.punta = 1; heads.push_back(e); }
        }
        if (tail && !tailUsada[i]){ tailUsada[i] = 1; BonePunta e; e.b = i; e.punta = 2; out.push_back(e); }
    }
    for (size_t k = 0; k < heads.size(); k++) out.push_back(heads[k]);
}

// E: extruye un hueso nuevo desde CADA punta seleccionada. Desde un tail: hijo CONECTADO que continua
// la direccion del padre (mismo largo). Desde un head raiz: hijo del hueso pero SIN conectar (queda la
// linea punteada), en la direccion opuesta. Sin seleccion usa el tail del hueso ACTIVO. Los nuevos
// quedan con su tail seleccionado (listos para arrastrar) y el ultimo pasa a ser el activo.
int BoneEditExtruir(Armature* a, std::vector<int>* outNuevos){
    if (outNuevos) outNuevos->clear();
    if (!a) return -1;
    std::vector<BonePunta> src;
    BonePuntasSeleccionadas(a, src);
    if (src.empty()){
        int act = a->boneActivo;
        if (act < 0 || act >= (int)a->bones.size()) return -1;
        BonePunta e; e.b = act; e.punta = 2; src.push_back(e);
    }
    UndoBonesCapturar(a);
    std::vector<int> nuevos;
    for (size_t k = 0; k < src.size(); k++){
        const W3dBone& p = a->bones[src[k].b];  // ojo: referencia valida (el push_back va despues de leerla)
        W3dBone nb;
        nb.name   = BoneNombreUnico(a, p.name, -1);
        nb.parent = src[k].b;
        if (src[k].punta == 2){ nb.head = p.tail; nb.tail = p.tail + (p.tail - p.head); }
        else                  { nb.head = p.head; nb.tail = p.head + (p.head - p.tail); }
        a->bones.push_back(nb);
        nuevos.push_back((int)a->bones.size() - 1);
    }
    BoneSelLimpiar(a);
    for (size_t k = 0; k < nuevos.size(); k++) a->bones[nuevos[k]].selTail = true; // el tip nuevo, como Blender
    a->boneActivo = nuevos.empty() ? -1 : nuevos[nuevos.size() - 1];
    if (outNuevos) *outNuevos = nuevos;
    BoneEditAplicado(a);
    return a->boneActivo;
}

// Shift+D: duplica los huesos SELECCIONADOS (o el activo). Los duplicados NO se conectan al original:
// conservan el padre solo si el padre TAMBIEN se duplico (se remapea a la copia); si no quedan raiz.
int BoneEditDuplicar(Armature* a){
    if (!a) return -1;
    std::vector<int> sel;
    for (size_t i = 0; i < a->bones.size(); i++)
        if (a->bones[i].select || (int)i == a->boneActivo) sel.push_back((int)i);
    if (sel.empty()) return -1;
    UndoBonesCapturar(a);
    std::vector<int> nuevoDe(a->bones.size(), -1); // idx original -> idx de su copia
    int actViejo = a->boneActivo, actNuevo = -1;
    size_t base = a->bones.size();
    for (size_t k = 0; k < sel.size(); k++){
        W3dBone nb = a->bones[sel[k]];             // copia por valor (head/tail/rest/pose)
        nb.name = BoneNombreUnico(a, nb.name, -1);
        nb.select = true; nb.selHead = true; nb.selTail = true;
        nuevoDe[sel[k]] = (int)a->bones.size();
        a->bones.push_back(nb);
    }
    // remapear padres ENTRE copias; el resto queda sin conectar (raiz)
    for (size_t i = base; i < a->bones.size(); i++){
        int pv = a->bones[i].parent;
        a->bones[i].parent = (pv >= 0 && pv < (int)nuevoDe.size() && nuevoDe[pv] >= 0) ? nuevoDe[pv] : -1;
    }
    for (size_t k = 0; k < sel.size(); k++){ // originales: deseleccionados
        a->bones[sel[k]].select = false; a->bones[sel[k]].selHead = false; a->bones[sel[k]].selTail = false;
    }
    actNuevo = (actViejo >= 0 && actViejo < (int)nuevoDe.size() && nuevoDe[actViejo] >= 0)
             ? nuevoDe[actViejo] : (int)a->bones.size() - 1;
    a->boneActivo = actNuevo;
    BoneEditSanearConectado(a);   // la copia que quedo RAIZ arrastraba el flag del original
    BoneEditAplicado(a);
    return actNuevo;
}

// X/Supr: borra los huesos seleccionados (o el activo). Los HIJOS sobrevivientes se re-parentan al
// primer ANCESTRO vivo (abuelo), o quedan raiz. Los tracks de los clips se remapean por indice
// (los del hueso borrado se van con el).
bool BoneEditBorrar(Armature* a){
    if (!a || a->bones.empty()) return false;
    int n = (int)a->bones.size();
    std::vector<char> del(n, 0);
    bool hay = false;
    for (int i = 0; i < n; i++)
        if (a->bones[i].select || i == a->boneActivo){ del[i] = 1; hay = true; }
    if (!hay) return false;
    UndoBonesCapturar(a);
    std::vector<int> ni(n, -1); // indice viejo -> nuevo
    int k = 0;
    for (int i = 0; i < n; i++) if (!del[i]) ni[i] = k++;
    std::vector<W3dBone> nuevos;
    nuevos.reserve(k);
    for (int i = 0; i < n; i++){
        if (del[i]) continue;
        W3dBone b = a->bones[i];
        int p = b.parent, guard = 0;
        while (p >= 0 && p < n && del[p] && guard++ < n) p = a->bones[p].parent; // sube al abuelo vivo
        b.parent = (p >= 0 && p < n) ? ni[p] : -1;
        nuevos.push_back(b);
    }
    // tracks de animacion: referencian huesos por INDICE -> remap (o fuera si su hueso se borro)
    for (size_t an = 0; an < a->animations.size(); an++){
        SkeletalAnimation* clip = a->animations[an];
        if (!clip) continue;
        for (size_t t = 0; t < clip->tracks.size(); ){
            int bt = clip->tracks[t].bone;
            if (bt >= 0 && bt < n && del[bt]) clip->tracks.erase(clip->tracks.begin() + t);
            else { if (bt >= 0 && bt < n) clip->tracks[t].bone = ni[bt]; ++t; }
        }
    }
    a->boneActivo = (a->boneActivo >= 0 && a->boneActivo < n && !del[a->boneActivo]) ? ni[a->boneActivo] : -1;
    a->bones.swap(nuevos);
    // la SELECCION DEL DOPE SHEET guarda el indice del hueso DENTRO de su clave
    // ("arm:#<serial rig>/k<CLIP>/b<IDX>") y sobrevive a esto: sin avisar, la clave vieja pasa a matchear la
    // fila de OTRO hueso y la tarjeta Keyframe / la 'X' del dope editan la curva equivocada (ver
    // Timeline.h). Se avisa de ARRIBA HACIA ABAJO: asi cada borrado corre solo los indices que
    // todavia no se movieron, que es exactamente el mapeo de ni[].
    // Y se avisa POR CADA CLIP: un hueso es del ARMATURE, no de un clip, asi que borrarlo corre los
    // indices de las claves de TODOS sus clips (la clave lleva el clip adentro desde la ronda 7).
    for (size_t c = 0; c < a->animations.size(); c++){
        char kb[24]; sprintf(kb, "/k%d/b", (int)c);   // "arm:#<serial>/k<CLIP>/b": el dueno va PRIMERO
        // el dueno va por SERIAL, no por nombre: el nombre se recicla al renombrar/borrar y el
        // aviso terminaria corriendo (o no corriendo) los indices del rig equivocado (ver Timeline.h).
        const std::string pref = "arm:" + DopeIdDueno(a) + kb;
        for (int i = n - 1; i >= 0; i--)
            if (del[i]) DopeRemapIndiceClave(pref, i, -1);
    }
    // el hijo que subio al ABUELO conserva su posicion: su head ya no coincide con el tail del padre nuevo,
    // asi que el flag 'conectado' heredado quedaria mintiendo. El saneador lo apaga (y lo deja prendido en
    // el caso raro en que las puntas SI coincidan). No mueve nada.
    BoneEditSanearConectado(a);
    BoneEditAplicado(a);
    return true;
}

// Alt+P > Clear Parent: los seleccionados (o el activo) quedan SIN padre (raiz). El hueso no se mueve.
bool BoneEditDesconectar(Armature* a){
    if (!a) return false;
    bool hay = false;
    for (size_t i = 0; i < a->bones.size(); i++)
        if ((a->bones[i].select || (int)i == a->boneActivo) && a->bones[i].parent >= 0) hay = true;
    if (!hay) return false;
    UndoBonesCapturar(a);
    for (size_t i = 0; i < a->bones.size(); i++)
        if (a->bones[i].select || (int)i == a->boneActivo) a->bones[i].parent = -1;
    BoneEditSanearConectado(a);   // sin padre no hay tail al que estar soldado: el flag no puede quedar en true
    BoneEditAplicado(a);
    return true;
}

// Alt+P > Disconnect Bone: los seleccionados (o el activo) SIGUEN emparentados pero pierden la
// soldadura (conectado = false): la punta compartida deja de ser una sola y al separarla queda
// la linea punteada con el padre. No mueve nada (posiciones intactas).
bool BoneEditDesconectarConectados(Armature* a){
    if (!a) return false;
    bool hay = false;
    for (size_t i = 0; i < a->bones.size(); i++)
        if ((a->bones[i].select || (int)i == a->boneActivo) &&
            a->bones[i].parent >= 0 && a->bones[i].conectado) hay = true;
    if (!hay) return false;
    UndoBonesCapturar(a);
    for (size_t i = 0; i < a->bones.size(); i++)
        if ((a->bones[i].select || (int)i == a->boneActivo) && a->bones[i].parent >= 0)
            a->bones[i].conectado = false;
    BoneEditAplicado(a);
    return true;
}

// SOLDAR (sin undo): mueve el hueso ENTERO para que su head caiga EXACTO en el tail del padre y
// prende el flag. Los descendientes CONECTADOS se mudan con el mismo delta (sino se romperia la
// soldadura de la cadena); los sueltos conservan su offset (keep offset, como el resto).
static void BoneSoldarRaw(Armature* a, int bone){
    W3dBone& b = a->bones[bone];
    int p = b.parent;
    if (p < 0 || p >= (int)a->bones.size()) return;
    Vector3 d = a->bones[p].tail - b.head;
    b.conectado = true;
    if (d.LengthSq() <= 0.0f) return;
    const int n = (int)a->bones.size();
    std::vector<char> mover(n, 0);
    mover[bone] = 1;
    bool cambio = true; int guard = 0;
    while (cambio && guard++ <= n){
        cambio = false;
        for (int c = 0; c < n; c++){
            if (mover[c]) continue;
            int pp = a->bones[c].parent;
            if (pp >= 0 && pp < n && mover[pp] && a->bones[c].conectado){ mover[c] = 1; cambio = true; }
        }
    }
    for (int i = 0; i < n; i++) if (mover[i]){ a->bones[i].head = a->bones[i].head + d; a->bones[i].tail = a->bones[i].tail + d; }
}

// ===== UNICO camino de CONECTAR / DESCONECTAR un hueso de su padre (flag 'conectado'). Lo
// comparten el checkbox "Connected" de la tarjeta Bones, el Ctrl+P > Connected y el Alt+P >
// Disconnect Bone. Conectar SUELDA (mueve el hueso); desconectar solo apaga el flag. =====
bool BoneSetConectado(Armature* a, int bone, bool con){
    if (!a || bone < 0 || bone >= (int)a->bones.size()) return false;
    if (a->bones[bone].parent < 0) return false;             // sin padre el flag no tiene sentido
    if (BoneEsConectado(a, bone) == con) return false;       // ya esta en ese estado EFECTIVO
    UndoBonesCapturar(a);
    if (con) BoneSoldarRaw(a, bone);
    else     a->bones[bone].conectado = false;
    BoneEditAplicado(a);
    return true;
}

// A / Alt+A en Edit de armature: selecciona TODOS los huesos (con sus dos puntas) o ninguno.
// Con la seleccion completa las ops (G/R/S, E, X, Shift+D, Ctrl+P) operan sobre todo.
bool BoneEditSeleccionarTodos(Armature* a, bool sel){
    if (!a || a->bones.empty()) return false;
    for (size_t i = 0; i < a->bones.size(); i++) BoneSelHueso(a, (int)i, sel);
    if (!sel) a->boneActivo = -1;
    else if (a->boneActivo < 0) a->boneActivo = 0;           // que quede un activo para las ops
    g_redraw = true;
    return true;
}

// Ctrl+P: los huesos SELECCIONADOS se emparentan al hueso ACTIVO. Los que armarian un ciclo (el
// activo desciende de ellos) se saltean. UN paso de undo para toda la operacion.
// conectar=false -> "Keep Offset" (no se mueve nada y quedan DESCONECTADOS -> linea punteada);
// conectar=true  -> "Connected" (el hijo se MUEVE para pegar su head al tail del activo, soldado).
// false = no habia nada que emparentar.
bool BoneParentarSeleccionAlActivo(Armature* a, bool conectar){
    if (!a) return false;
    int act = a->boneActivo;
    if (act < 0 || act >= (int)a->bones.size()) return false;
    std::vector<int> aParentar;
    for (size_t i = 0; i < a->bones.size(); i++){
        if ((int)i == act || !a->bones[i].select) continue;
        if (BoneEsDescendiente(a, (int)i, act)) continue;    // el activo desciende de i -> ciclo
        if (!conectar && a->bones[i].parent == act && !a->bones[i].conectado) continue; // ya esta asi
        if (conectar && a->bones[i].parent == act && BoneEsConectado(a, (int)i)) continue; // ya esta asi
        aParentar.push_back((int)i);
    }
    if (aParentar.empty()) return false;
    UndoBonesCapturar(a);
    for (size_t k = 0; k < aParentar.size(); k++){
        a->bones[aParentar[k]].parent = act;
        a->bones[aParentar[k]].conectado = false;            // keep offset (sin soldar)
        if (conectar) BoneSoldarRaw(a, aParentar[k]);        // Connected: pega el head al tail del padre
    }
    BoneEditSanearConectado(a);   // red de seguridad del invariante (los descendientes tambien se movieron)
    BoneEditAplicado(a);
    return true;
}

// 'b' es el mismo 'ancestro' o desciende de el? (para excluir candidatos que armarian un ciclo)
bool BoneEsDescendiente(Armature* a, int ancestro, int b){
    if (!a || b < 0 || b >= (int)a->bones.size()) return false;
    if (b == ancestro) return true;
    int p = a->bones[b].parent, guard = 0;
    while (p >= 0 && p < (int)a->bones.size() && guard++ < (int)a->bones.size()){
        if (p == ancestro) return true;
        p = a->bones[p].parent;
    }
    return false;
}

// re-parenta 'bone' a 'nuevoPadre' (-1 = raiz). Rechaza ciclos (el nuevo padre no puede ser el mismo
// hueso ni un descendiente). El hueso NO se mueve (como Desconectar). Lo usa el desplegable "Parent"
// de la tarjeta Bones y el harness de tests.
bool BoneReparentar(Armature* a, int bone, int nuevoPadre){
    if (!a || bone < 0 || bone >= (int)a->bones.size()) return false;
    if (nuevoPadre >= (int)a->bones.size()) return false;
    if (nuevoPadre < 0) nuevoPadre = -1;
    if (nuevoPadre == a->bones[bone].parent) return false;
    if (nuevoPadre >= 0 && BoneEsDescendiente(a, bone, nuevoPadre)) return false; // ciclo
    UndoBonesCapturar(a);
    a->bones[bone].parent = nuevoPadre;
    if (nuevoPadre >= 0) a->bones[bone].conectado = false; // keep offset: emparentado SIN soldar
    BoneEditSanearConectado(a);   // y si quedo raiz (-1) el flag tampoco puede sobrevivir
    BoneEditAplicado(a);
    return true;
}

// mueve head/tail SIN undo (el caller envuelve con UndoBonesIniciar/Confirmar o Capturar)
void BoneEditMoverExtremo(Armature* a, int bone, int mask, const Vector3& delta){
    if (!a || bone < 0 || bone >= (int)a->bones.size()) return;
    if (mask & 1) a->bones[bone].head = a->bones[bone].head + delta;
    if (mask & 2) a->bones[bone].tail = a->bones[bone].tail + delta;
    BoneEditAplicado(a);
}

// los vertex groups homonimos de las mallas ligadas a 'a' (misma regla de "ligada" que
// BoneRenombrarGrupos). Se juntan ANTES de escribir, para capturarlos en UN paso de undo.
void JuntarVGroupsHomonimos(Object* nodo, Armature* a, const std::string& nombre, std::vector<W3dRenameDest>& out){
    if (!nodo) return;
    if (nodo->getType() == ObjectType::mesh){
        Mesh* m = (Mesh*)nodo;
        bool delRig = (m->skinArmature == a);
        if (!delRig)
            for (size_t i = 0; i < m->modificadores.size() && !delRig; i++)
                if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature &&
                    m->modificadores[i]->target == (Object*)a) delRig = true;
        if (delRig)
            for (size_t g = 0; g < m->vertexGroups.size(); g++)
                if (m->vertexGroups[g] && m->vertexGroups[g]->nombre == nombre)
                    out.push_back(W3dDestCapaMalla(m, W3dRenameDest::VGroup, (int)g));
    }
    for (size_t i = 0; i < nodo->Childrens.size(); i++)
        JuntarVGroupsHomonimos(nodo->Childrens[i], a, nombre, out);
}

// TODOS los nombres de vertex group de las mallas ligadas a 'a' (misma regla de "ligada"), SALVO
// los homonimos de 'salvo' (esos son los que el rename va a ARRASTRAR: no son un choque). Con esto
// el nombre nuevo del hueso se elige libre en LAS DOS PUNTAS del binding de una sola vez.
static void JuntarVGroupsNombres(Object* nodo, Armature* a, const std::string& salvo, std::vector<std::string>& out){
    if (!nodo) return;
    if (nodo->getType() == ObjectType::mesh){
        Mesh* m = (Mesh*)nodo;
        bool delRig = (m->skinArmature == a);
        if (!delRig)
            for (size_t i = 0; i < m->modificadores.size() && !delRig; i++)
                if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature &&
                    m->modificadores[i]->target == (Object*)a) delRig = true;
        if (delRig)
            for (size_t g = 0; g < m->vertexGroups.size(); g++)
                if (m->vertexGroups[g] && m->vertexGroups[g]->nombre != salvo)
                    out.push_back(m->vertexGroups[g]->nombre);
    }
    for (size_t i = 0; i < nodo->Childrens.size(); i++)
        JuntarVGroupsNombres(nodo->Childrens[i], a, salvo, out);
}

// nombre libre para el hueso 'idx' contando LAS DOS PUNTAS: los otros huesos del armature y (solo
// si el rename va a arrastrar vertex groups homonimos) los vertex groups de las mallas ligadas.
// Sin arrastre no se miran los grupos: ponerle a un hueso el nombre de un grupo existente es la
// forma de LIGARLOS a mano.
static std::string BoneNombreUnicoBind(Armature* a, const std::string& base, int idx, const std::string& viejo){
    if (!a) return base;
    std::vector<W3dRenameDest> homs; JuntarVGroupsHomonimos(SceneCollection, a, viejo, homs);
    std::vector<std::string> tomados;
    for (size_t i = 0; i < a->bones.size(); i++)
        if ((int)i != idx) tomados.push_back(a->bones[i].name);
    if (!homs.empty()) JuntarVGroupsNombres(SceneCollection, a, viejo, tomados);
    return W3dNombreUnicoEnValores(base, "Bone", tomados, -1);
}

// renombra el hueso (unico) Y el vertex group asociado (mismo nombre) en las mallas ligadas a este
// armature: el binding malla<->hueso es POR NOMBRE (SkinearMesh), renombrar uno sin el otro lo rompe.
static int BoneRenombrarGrupos(Object* nodo, Armature* a, const std::string& viejo, const std::string& nuevo){
    if (!nodo) return 0;
    int n = 0;
    if (nodo->getType() == ObjectType::mesh){
        Mesh* m = (Mesh*)nodo;
        bool delRig = (m->skinArmature == a);
        if (!delRig) // tambien las mallas con el modificador apuntando al armature (aunque este apagado)
            for (size_t i = 0; i < m->modificadores.size() && !delRig; i++)
                if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature &&
                    m->modificadores[i]->target == (Object*)a) delRig = true;
        if (delRig)
            for (size_t g = 0; g < m->vertexGroups.size(); g++)
                if (m->vertexGroups[g]->nombre == viejo){
                    // el paso de undo lo captura el CALLER (BoneRenombrar / BoneRenombradoDesdeUI)
                    // para las DOS puntas juntas: en pasos separados el Ctrl+Z intermedio dejaba el
                    // binding roto (misma leccion que Bone2DRenameUndo del rig 2D)
                    m->vertexGroups[g]->nombre = nuevo;
                    m->lastSkinFrame = -999999; // el CSR de skinning hashea los nombres -> se reconstruye solo
                    n++;
                }
    }
    for (size_t i = 0; i < nodo->Childrens.size(); i++)
        n += BoneRenombrarGrupos(nodo->Childrens[i], a, viejo, nuevo);
    return n;
}
// tras un rename hecho por la UI (la tarjeta Bones ya escribio el nombre nuevo en el hueso):
// sincroniza los vertex groups viejo->nuevo + avisa + re-prepara. 'viejo' = el nombre previo.
void BoneRenombradoDesdeUI(Armature* a, int idx, const std::string& viejo){
    if (!a || idx < 0 || idx >= (int)a->bones.size()) return;
    const std::string nuevo = a->bones[idx].name;
    if (viejo == nuevo || nuevo.empty()) return;
    // los grupos, en UN paso de undo (el hueso ya lo escribio la UI con su propio snapshot)
    { std::vector<W3dRenameDest> v; JuntarVGroupsHomonimos(SceneCollection, a, viejo, v);
      UndoCapturarRenames(v); }
    int nvg = BoneRenombrarGrupos(SceneCollection, a, viejo, nuevo);
    if (nvg > 0) W3dAvisoArrastre(nvg, "vertex group(s)", viejo, nuevo);
    BoneEditAplicado(a);
}

bool BoneRenombrar(Armature* a, int idx, const std::string& nuevo){
    if (!a || idx < 0 || idx >= (int)a->bones.size() || nuevo.empty()) return false;
    std::string viejo = a->bones[idx].name;
    if (viejo == nuevo) return false;
    // el nombre tiene que quedar libre en LAS DOS PUNTAS: uniquificar solo entre huesos dejaba DOS
    // vertex groups homonimos cuando el nombre pedido ya era de otro grupo de la malla ligada.
    std::string unico = BoneNombreUnicoBind(a, nuevo, idx, viejo);
    // UN SOLO paso de undo para las DOS puntas del binding (hueso + vertex groups
    // homonimos): en pasos separados, el primer Ctrl+Z devolvia el nombre del hueso y
    // dejaba los grupos con el nuevo -> el rig no deformaba hasta el segundo Ctrl+Z.
    UndoBonesCapturar(a); // snapshot de bones (incluye el nombre viejo)
    { std::vector<W3dRenameDest> v; JuntarVGroupsHomonimos(SceneCollection, a, viejo, v);
      // OJO: fundir SOLO si hubo grupos que capturar; si no, UndoFundirUltimos se
      // comeria el paso anterior (que no tiene nada que ver con este rename).
      if (!v.empty()) { UndoCapturarRenames(v); UndoFundirUltimos(2); } }
    a->bones[idx].name = unico;
    int nvg = BoneRenombrarGrupos(SceneCollection, a, viejo, unico);
    if (nvg > 0) W3dAvisoArrastre(nvg, "vertex group(s)", viejo, unico);
    BoneEditAplicado(a);
    return true;
}

// ---------------------------------------------------------------------------------------------------
//  GRAB / TRANSFORM MODAL: drag de puntas (o huesos enteros) con delta ABSOLUTO desde el snapshot.
//  Dos sabores:
//   - porClick / UV: solo TRASLADA en el plano de la vista (o en UV), sin ejes (g_grabEjes=false).
//   - modal 3D (BoneXformStart, G/R/S): usa estado/axisSelect/transformOrientation + el pivote del
//     viewport (g_transformPivot) -> mismos ejes, orientaciones, barra y NumInput que Object/Pose.
// ---------------------------------------------------------------------------------------------------
struct BoneGrabEntrada { int b; int mask; Vector3 head0, tail0; Vector3 piv; }; // piv = pivote propio (Individual)
static std::vector<BoneGrabEntrada> g_grab;
static Armature* g_grabArm = NULL;
static bool g_grabActivo = false;
static bool g_grabClick  = false;  // true: termina al soltar el mouse; false: modal (click/Enter confirma)
static Vector3 g_grabAcc;          // delta acumulado en espacio LOCAL del armature (sabor sin ejes)
static int g_grabLastX = 0, g_grabLastY = 0;
static bool g_grabConsumioClick = false; // el click que CONFIRMO un grab no debe re-pickear
// --- estado del modal con ejes (G/R/S del 3D) ---
static int  g_grabModo = 1;        // 1=grab 2=rotate 3=scale
static bool g_grabEjes = false;    // true: el modal usa estado/axisSelect/transformOrientation
static Vector3 g_grabPivotLocal;   // pivote comun en espacio LOCAL del armature
static Matrix4 g_grabW, g_grabInvW;// world del armature al arrancar (local<->mundo)
static float g_grabAccX = 0.0f, g_grabAccY = 0.0f; // delta de mouse acumulado (modal con ejes)
static bool  g_grabTrackCap = false; static float g_grabTrackAng0 = 0.0f; // trackball (rotate libre)
static bool  g_grabNum = false; static float g_grabNumV = 0.0f; // valor exacto tipeado (NumInput)

static Matrix4 BMatTrans(const Vector3& t){ Matrix4 m; m.Identity(); m.m[12]=t.x; m.m[13]=t.y; m.m[14]=t.z; return m; }
// direccion mundo -> LOCAL del armature (resta el origen; SIN normalizar: respeta la escala del objeto)
static Vector3 BDirMundoALocal(const Vector3& d){
    return (g_grabInvW * d) - (g_grabInvW * Vector3(0, 0, 0));
}
static Vector3 BCam(int which){ // ejes de camara en mundo: 0=right 1=up 2=forward
    Quaternion vr = Viewport3DActive ? Viewport3DActive->viewRot : Quaternion(1,0,0,0);
    return vr * ((which==0)?Vector3(1,0,0):(which==1)?Vector3(0,1,0):Vector3(0,0,-1));
}
// eje X/Y/Z (0/1/2, convencion usuario) en MUNDO segun transformOrientation. Global: X=(1,0,0),
// Y=(0,0,1) profundidad, Z=(0,1,0) arriba (misma convencion que EjeMundo/Pose). View: right/fwd/up.
// Local: la base del HUESO ACTIVO con Y A LO LARGO del hueso (los edit-bones no tienen rotacion
// propia: el eje util es su direccion, como en Blender).
static Vector3 BoneAxisMundo(int ax){
    if (transformOrientation == LocalOrient && g_grabArm){
        Armature* a = g_grabArm;
        int act = a->boneActivo;
        if (act >= 0 && act < (int)a->bones.size()){
            Vector3 hw = g_grabW * a->bones[act].head, tw = g_grabW * a->bones[act].tail;
            Vector3 ey = tw - hw; float l = ey.Length();
            if (l > 1e-6f){
                ey = ey * (1.0f / l);
                Vector3 ex = Vector3::Cross(ey, Vector3(0, 1, 0));
                if (ex.LengthSq() < 1e-8f) ex = Vector3::Cross(ey, Vector3(1, 0, 0));
                ex = ex.Normalized();
                Vector3 ez = Vector3::Cross(ex, ey);
                return (ax == 0) ? ex : (ax == 1) ? ey : ez;
            }
        }
    }
    if (transformOrientation == ViewOrient) return (ax==0)?BCam(0):(ax==1)?BCam(2):BCam(1);
    return (ax==0)?Vector3(1,0,0):(ax==1)?Vector3(0,0,1):Vector3(0,1,0); // GLOBAL (engine Y-up)
}

// puntos EN MOVIMIENTO del snapshot, sin repetir la punta compartida (el head conectado cuyo padre
// tambien mueve el tail cuenta una sola vez): para la mediana del pivote.
static Vector3 BoneGrabMediana(Armature* a){
    Vector3 acc(0,0,0); int n = 0;
    std::vector<char> tailMueve(a->bones.size(), 0);
    for (size_t k = 0; k < g_grab.size(); k++)
        if ((g_grab[k].mask & 2) && g_grab[k].b >= 0 && g_grab[k].b < (int)a->bones.size())
            tailMueve[g_grab[k].b] = 1;
    for (size_t k = 0; k < g_grab.size(); k++){
        BoneGrabEntrada& e = g_grab[k];
        if (e.b < 0 || e.b >= (int)a->bones.size()) continue;
        if (e.mask & 1){
            int p = a->bones[e.b].parent;
            bool compartida = BoneEsConectado(a, e.b) && p >= 0 && p < (int)a->bones.size() && tailMueve[p];
            if (!compartida){ acc = acc + e.head0; n++; }
        }
        if (e.mask & 2){ acc = acc + e.tail0; n++; }
    }
    return (n > 0) ? acc * (1.0f / (float)n) : acc;
}
// pivote comun en espacio LOCAL segun g_transformPivot (cursor 3D / hueso activo / mediana).
// Individual usa los pivotes POR ENTRADA (e.piv); este igual se calcula para la linea/trackball.
static Vector3 BoneGrabPivotLocal(Armature* a){
    if (g_transformPivot == PivotCursor3D) return g_grabInvW * cursor3D.pos;
    if (g_transformPivot == PivotActive && a->boneActivo >= 0 && a->boneActivo < (int)a->bones.size()){
        W3dBone& b = a->bones[a->boneActivo];
        if (b.selHead && !b.selTail && !b.select) return b.head;   // la punta elegida manda
        if (b.selTail && !b.selHead && !b.select) return b.tail;
        return (b.head + b.tail) * 0.5f;                            // hueso entero: su centro
    }
    return BoneGrabMediana(a);
}

// arranque comun. modo/conEjes: ver arriba. porClick solo tiene sentido con modo=1.
static void BoneGrabStartEx(Armature* a, const std::vector<int>& bones, const std::vector<int>& masks,
                            bool porClick, int modo, bool conEjes){
    if (!a || bones.empty() || g_grabActivo) return;
    g_grab.clear();
    for (size_t k = 0; k < bones.size(); k++){
        int b = bones[k];
        if (b < 0 || b >= (int)a->bones.size()) continue;
        BoneGrabEntrada e;
        e.b = b;
        e.mask = (k < masks.size()) ? masks[k] : 3;
        e.head0 = a->bones[b].head; e.tail0 = a->bones[b].tail;
        // pivote propio (modo Individual): la mediana de SUS puntos en movimiento
        if (e.mask == 3)      e.piv = (e.head0 + e.tail0) * 0.5f;
        else if (e.mask == 1) e.piv = e.head0;
        else                  e.piv = e.tail0;
        g_grab.push_back(e);
    }
    if (g_grab.empty()) return;
    UndoBonesIniciar(a); // pendiente: se pushea al confirmar (si cambio)
    g_grabArm = a; g_grabActivo = true; g_grabClick = porClick;
    g_grabModo = modo; g_grabEjes = conEjes;
    g_grabAcc = Vector3(0, 0, 0);
    g_grabAccX = g_grabAccY = 0.0f;
    g_grabTrackCap = false; g_grabNum = false; g_grabNumV = 0.0f;
    g_grabLastX = lastMouseX; g_grabLastY = lastMouseY;
    // EFECTIVA a proposito (no GetWorldMatrixBase): g_grabW/g_grabInvW no hornean nada, MIDEN
    // EN PANTALLA. Con la inversa se pasa a local una DIRECCION que ya viene de la camara (el
    // arrastre del mouse, BDirMundoALocal / BoneAxisMundo) y con la directa se dibuja el pivote,
    // asi que tiene que ser la MISMA matriz con la que se dibujan los huesos o el hueso no sigue
    // al mouse. Lo que se escribe en head/tail sale del arrastre, no de la transform del objeto.
    // Justamente PORQUE es efectiva hay que decir de que vista: la matriz efectiva sale de la
    // ultima bindeada y con dos viewports abiertos esa puede ser la del OTRO (el grab arranca por
    // teclado, sin dibujar nada en el medio). Se bindea el viewport donde el usuario esta.
    if (Viewport3DActive) Viewport3DActive->BindVista();
    a->GetWorldMatrix(g_grabW);
    g_grabInvW = g_grabW.Inverse();
    g_grabPivotLocal = BoneGrabPivotLocal(a);
    if (conEjes){
        // pivote en MUNDO: ahi se dibujan la linea de eje, la punteada y se mide el trackball
        TransformPivotPoint = g_grabW * g_grabPivotLocal;
        // estado COMPARTIDO con Object/Pose -> barra de estado + numerico + tilde/cruz de la toolbar
        estado = (modo == 1) ? translacion : (modo == 2) ? rotacion : EditScale;
        axisSelect = (modo == 3) ? XYZ : ViewAxis;
        transformOrientation = (modo == 3) ? LocalOrient : GlobalOrient;
        gAnguloTransform = 0.0f;
        NumInputReset();
    }
}

void BoneGrabStart(Armature* a, const std::vector<int>& bones, const std::vector<int>& masks, bool porClick){
    BoneGrabStartEx(a, bones, masks, porClick, 1, false);
}
void BoneGrabStartSeleccion(Armature* a, bool porClick){
    std::vector<int> bones, masks;
    if (!BoneSeleccionMasks(a, bones, masks)) return;
    BoneGrabStartEx(a, bones, masks, porClick, 1, false);
}
bool BoneGrabActivo(){ return g_grabActivo; }
bool BoneGrabPorClick(){ return g_grabActivo && g_grabClick; }
int  BoneXformModo(){ return (g_grabActivo && g_grabEjes) ? g_grabModo : 0; }

// G/R/S del viewport 3D: transform modal de la seleccion con ejes y pivote
void BoneXformStart(Armature* a, int modo){
    if (!a || g_grabActivo) return;
    if (estado != editNavegacion) return; // ya hay un transform en curso
    std::vector<int> bones, masks;
    if (!BoneSeleccionMasks(a, bones, masks)) return;
    BoneGrabStartEx(a, bones, masks, false, modo, true);
}

// aplica el sabor SIN ejes (drag por click / UV): traslacion pura del delta acumulado
static void BoneGrabAplicar(){
    if (!g_grabActivo || !g_grabArm) return;
    for (size_t k = 0; k < g_grab.size(); k++){
        BoneGrabEntrada& e = g_grab[k];
        if (e.b < 0 || e.b >= (int)g_grabArm->bones.size()) continue;
        if (e.mask & 1) g_grabArm->bones[e.b].head = e.head0 + g_grabAcc;
        if (e.mask & 2) g_grabArm->bones[e.b].tail = e.tail0 + g_grabAcc;
    }
    BoneEditAplicado(g_grabArm);
}

// re-aplica el transform MODAL desde el snapshot (absoluto -> sin drift). Traduce el delta de mouse
// (o el valor numerico) al espacio LOCAL del armature segun modo/eje/orientacion y pivote.
static void BoneXformAplicar(){
    if (!g_grabActivo || !g_grabArm || !g_grabEjes) return;
    Armature* a = g_grabArm;
    bool axFree  = (axisSelect == ViewAxis || axisSelect == XYZ);
    bool esPlano = (axisSelect == PlaneX || axisSelect == PlaneY || axisSelect == PlaneZ);
    int  ax = (axisSelect==X||axisSelect==PlaneX)?0 : (axisSelect==Y||axisSelect==PlaneY)?1 : (axisSelect==Z||axisSelect==PlaneZ)?2 : 0;
    float wpp = Viewport3DActive ? Viewport3DActive->VelocidadArrastreMundo() : 0.01f;

    Vector3 tLocal(0, 0, 0);       // modo 1
    Matrix4 M; M.Identity();       // modos 2/3: la parte rotacion/escala (el pivote se compone aparte)
    if (g_grabModo == 1){          // ---- TRANSLATE ----
        if (axFree){
            if (g_grabNum) tLocal = BDirMundoALocal(BCam(0)) * g_grabNumV;
            else tLocal = BDirMundoALocal(BCam(0)) * (g_grabAccX * wpp)
                        + BDirMundoALocal(BCam(1)) * (-g_grabAccY * wpp);
        } else if (esPlano){       // los OTROS dos ejes
            int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
            float m1 = g_grabNum ? g_grabNumV : (g_grabAccX * wpp);
            float m2 = g_grabNum ? 0.0f       : (-g_grabAccY * wpp);
            tLocal = BDirMundoALocal(BoneAxisMundo(a1)) * m1 + BDirMundoALocal(BoneAxisMundo(a2)) * m2;
        } else {                   // eje unico
            float mag = g_grabNum ? g_grabNumV : ((g_grabAccX - g_grabAccY) * wpp);
            tLocal = BDirMundoALocal(BoneAxisMundo(ax)) * mag;
        }
    } else if (g_grabModo == 2){   // ---- ROTATE alrededor del pivote ----
        // TRACKBALL (libre): el angulo lo da la posicion del mouse respecto del PIVOTE proyectado
        // (360 reales), igual que Object/Pose. Con eje constrenido: arrastre horizontal.
        float ang;
        if (g_grabNum) ang = g_grabNumV;
        else if (axFree && Viewport3DActive){
            float px, py;
            if (Viewport3DActive->ProyectarPunto(TransformPivotPoint, px, py)){
                float lmx = (float)g_grabLastX - (float)Viewport3DActive->x;
                float lmy = (float)g_grabLastY - (float)Viewport3DActive->y;
                float aM = 180.0f - atan2f(lmy - py, lmx - px) * 180.0f / 3.14159265f;
                if (!g_grabTrackCap){ g_grabTrackCap = true; g_grabTrackAng0 = aM; }
                ang = g_grabTrackAng0 - aM; // negado: el hueso sigue al mouse (igual que Object/Pose)
            } else ang = g_grabAccX * 0.5f;
        } else ang = g_grabAccX * 0.5f;
        gAnguloTransform = ang; // barra de estado
        Vector3 axisW = axFree ? BCam(2) : BoneAxisMundo(ax);
        Vector3 axisL = BDirMundoALocal(axisW);
        float l = axisL.Length(); if (l > 1e-8f) axisL = axisL * (1.0f / l);
        M = Quaternion::FromAxisAngle(axisL, ang).ToMatrix();
    } else {                       // ---- SCALE alrededor del pivote ----
        float f = g_grabNum ? g_grabNumV : (1.0f + g_grabAccX * 0.01f);
        if (!g_grabNum && f < 0.01f) f = 0.01f;
        if (axFree){ M.m[0] = f; M.m[5] = f; M.m[10] = f; }
        else {       // escala f a lo largo del eje n:  I + (f-1)*n n^T   (m[col*4+fila], column-major)
            Vector3 n = BDirMundoALocal(BoneAxisMundo(ax));
            float l = n.Length(); if (l > 1e-8f) n = n * (1.0f / l);
            float g = f - 1.0f;
            M.m[0]=1+g*n.x*n.x; M.m[1]=g*n.x*n.y;   M.m[2]=g*n.x*n.z;
            M.m[4]=g*n.y*n.x;   M.m[5]=1+g*n.y*n.y; M.m[6]=g*n.y*n.z;
            M.m[8]=g*n.z*n.x;   M.m[9]=g*n.z*n.y;   M.m[10]=1+g*n.z*n.z;
        }
    }
    // aplicar sobre el snapshot. Individual: cada hueso alrededor de SU pivote (rotar/escalar)
    bool indiv = (g_transformPivot == PivotIndividual && g_grabModo != 1);
    Matrix4 Delta;
    if (g_grabModo != 1 && !indiv)
        Delta = BMatTrans(g_grabPivotLocal) * M * BMatTrans(g_grabPivotLocal * -1.0f);
    for (size_t k = 0; k < g_grab.size(); k++){
        BoneGrabEntrada& e = g_grab[k];
        if (e.b < 0 || e.b >= (int)a->bones.size()) continue;
        if (g_grabModo == 1){
            if (e.mask & 1) a->bones[e.b].head = e.head0 + tLocal;
            if (e.mask & 2) a->bones[e.b].tail = e.tail0 + tLocal;
        } else {
            Matrix4 D = indiv ? (BMatTrans(e.piv) * M * BMatTrans(e.piv * -1.0f)) : Delta;
            if (e.mask & 1) a->bones[e.b].head = D * e.head0;
            if (e.mask & 2) a->bones[e.b].tail = D * e.tail0;
        }
    }
    BoneEditAplicado(a);
}

// 3D sin ejes (drag por click): el delta de mouse va al plano de la vista (world por pixel a la
// profundidad del pivot) y de ahi al espacio LOCAL del armature. Con ejes: acumula y re-aplica.
void BoneGrabMotion3D(Viewport3D* vp, int mx, int my){
    if (!g_grabActivo || !g_grabArm || !vp) return;
    extern bool NumInputActivo();
    if (g_grabEjes && NumInputActivo()){ // tipeando un valor exacto: el mouse no interfiere
        g_grabLastX = mx; g_grabLastY = my;
        vp->ActualizarLineaTransform(mx, my);
        return;
    }
    float dx = (float)(mx - g_grabLastX), dy = (float)(my - g_grabLastY);
    g_grabLastX = mx; g_grabLastY = my;
    if (dx == 0.0f && dy == 0.0f) return;
    if (g_grabEjes){
        g_grabNum = false;
        g_grabAccX += dx; g_grabAccY += dy;
        vp->ActualizarLineaTransform(mx, my); // linea punteada pivote->mouse (rotar/escalar)
        BoneXformAplicar();
        return;
    }
    float wpp = vp->VelocidadArrastreMundo();
    Vector3 wd = (vp->viewRot * Vector3(1, 0, 0)) * (dx * wpp)
               + (vp->viewRot * Vector3(0, 1, 0)) * (-dy * wpp);
    // EFECTIVA por el mismo motivo que en BoneGrabStartEx (ver el comentario de g_grabW): 'wd'
    // ya viene de la camara de ESTE viewport y se baja a local como DIRECCION, o sea que es una
    // medicion en pantalla, no un horneado. Y como es efectiva hay que decir de QUE vista: la
    // matriz efectiva sale de la ultima bindeada, y con dos viewports abiertos el otro pudo
    // dibujar entre dos motions. Sin este bind el MISMO arrastre reparte su delta entre dos
    // espacios locales distintos... y lo que termina escrito (y guardado) son head/tail.
    vp->BindVista();
    Matrix4 W; g_grabArm->GetWorldMatrix(W);
    Matrix4 invW = W.Inverse();
    Vector3 dLocal = (invW * wd) - (invW * Vector3(0, 0, 0)); // como DIRECCION
    g_grabAcc = g_grabAcc + dLocal;
    BoneGrabAplicar();
}
// UV (armature 2D): delta ABSOLUTO en UV desde el arranque; los huesos viven en ese espacio (Z fijo).
void BoneGrabMotionUV(float du, float dv){
    if (!g_grabActivo || g_grabEjes) return;
    g_grabAcc = Vector3(du, dv, 0.0f);
    BoneGrabAplicar();
}
// valor numerico exacto (lo llama NumInputAplicar con un modal de huesos activo)
void BoneXformNumValor(float v){
    if (!g_grabActivo || !g_grabEjes) return;
    g_grabNum = true; g_grabNumV = v;
    BoneXformAplicar();
}
// re-aplica con el eje/orientacion nuevos SIN resetear el arrastre (toolbar de ejes / menu Orient)
void BoneXformReaplicar(){
    if (!g_grabActivo || !g_grabEjes) return;
    g_grabTrackCap = false;
    BoneXformAplicar();
}
// para la barra de estado: el valor del transform en curso (grab: magnitud; scale: factor)
float BoneXformHeaderValor(){
    if (!g_grabActivo || !g_grabEjes) return 0.0f;
    if (g_grabNum) return g_grabNumV;
    float wpp = Viewport3DActive ? Viewport3DActive->VelocidadArrastreMundo() : 0.01f;
    if (g_grabModo == 1){
        bool axFree = (axisSelect == ViewAxis || axisSelect == XYZ);
        return axFree ? (Vector3(g_grabAccX, g_grabAccY, 0.0f).Length() * wpp)
                      : ((g_grabAccX - g_grabAccY) * wpp);
    }
    if (g_grabModo == 3){ float f = 1.0f + g_grabAccX * 0.01f; return (f < 0.01f) ? 0.01f : f; }
    return gAnguloTransform; // rotate (la barra igual lo toma de gAnguloTransform)
}

// X/Y/Z durante el modal: constrinie el eje (1er toque) y cicla Global->Local->View al re-apretar,
// luego RE-APLICA desde el snapshot. Misma logica que PoseCiclarEje.
void BoneCiclarEje(int eje){
    if (!BoneXformModo()) return;
    if (g_grabModo == 3){ transformOrientation = LocalOrient; axisSelect = (axisSelect==eje)?XYZ:eje; } // scale: local, eje<->3ejes
    else if (axisSelect != eje){ axisSelect = eje; }                                                    // 1er toque: eje (orientacion actual)
    else if (transformOrientation == GlobalOrient){ transformOrientation = LocalOrient; }               // mismo eje: cicla orientacion
    else if (transformOrientation == LocalOrient){ transformOrientation = ViewOrient; }
    else { axisSelect = ViewAxis; transformOrientation = GlobalOrient; }                                // vuelve a libre
    g_grabAccX = g_grabAccY = 0.0f; g_grabLastX = lastMouseX; g_grabLastY = lastMouseY;
    g_grabNum = false; g_grabTrackCap = false; NumInputReset();
    BoneXformAplicar();
}
void BoneCiclarPlano(int eje){ // Shift+X/Y/Z: constrinie al PLANO (los otros dos ejes). Rotate no tiene plano.
    if (!BoneXformModo() || g_grabModo == 2) return;
    int pl = (eje==0)?PlaneX:(eje==1)?PlaneY:PlaneZ;
    axisSelect = (axisSelect==pl)?ViewAxis:pl; transformOrientation = GlobalOrient;
    g_grabAccX = g_grabAccY = 0.0f; g_grabLastX = lastMouseX; g_grabLastY = lastMouseY;
    g_grabNum = false; g_grabTrackCap = false; NumInputReset();
    BoneXformAplicar();
}
void BoneCiclarOrient(){ // "R de nuevo": cicla la orientacion del eje actual (Global->Local->View)
    if (!BoneXformModo()) return;
    transformOrientation = (transformOrientation==GlobalOrient)?LocalOrient:(transformOrientation==LocalOrient)?ViewOrient:GlobalOrient;
    g_grabAccX = g_grabAccY = 0.0f; g_grabLastX = lastMouseX; g_grabLastY = lastMouseY; g_grabTrackCap = false;
    BoneXformAplicar();
}
// EJES del modal en MUNDO, para que la GUIA (DibujarLineaEjeMundo) dibuje EXACTAMENTE el mismo eje
// con el que se transforma (en Local son los del HUESO activo, no los del objeto armature).
bool BoneEjesMundo(Vector3& ex, Vector3& ey, Vector3& ez){
    if (!BoneXformModo()) return false;
    ex = BoneAxisMundo(0); ey = BoneAxisMundo(1); ez = BoneAxisMundo(2);
    return true;
}

static void BoneGrabTerminado(){
    g_grabActivo = false; g_grabArm = NULL; g_grab.clear();
    if (g_grabEjes){ estado = editNavegacion; NumInputReset(); }
    g_grabEjes = false; g_grabModo = 1;
}
void BoneGrabConfirmar(){
    if (!g_grabActivo) return;
    Armature* a = g_grabArm;
    BoneGrabTerminado();
    UndoBonesConfirmar(); // pushea el paso (o lo descarta si no cambio nada)
    if (a) BoneEditAplicado(a);
}
void BoneGrabCancelar(){
    if (!g_grabActivo) return;
    Armature* a = g_grabArm;
    for (size_t k = 0; k < g_grab.size(); k++){
        BoneGrabEntrada& e = g_grab[k];
        if (!a || e.b < 0 || e.b >= (int)a->bones.size()) continue;
        a->bones[e.b].head = e.head0; a->bones[e.b].tail = e.tail0;
    }
    BoneGrabTerminado();
    UndoBonesCancelar();
    if (a) BoneEditAplicado(a);
}

// ---------------------------------------------------------------------------------------------------
//  pick + click del 3D (Edit-de-armature)
// ---------------------------------------------------------------------------------------------------
// pick sobre head/tail CRUDOS (rest autorada), no la pose: en Edit se edita el rest. Las PUNTAS
// tienen prioridad (radio propio en px); si ninguna cae cerca, gana el CUERPO mas cercano.
int BonePickEdit(Viewport3D* vp, Armature* a, float lmx, float lmy, int* outMask){
    if (outMask) *outMask = 3;
    if (!vp || !a || a->bones.empty()) return -1;
    vp->BindVista();   // el pick proyecta con la camara de ESTE viewport (ver BoneGrabStartSeleccion)
    Matrix4 W; a->GetWorldMatrix(W);
    int bestP = -1, bestPMask = 0; float bestPD = 12.0f;  // mejor PUNTA (radio 12 px)
    int bestB = -1;                float bestBD = 14.0f;  // mejor CUERPO (umbral como PickBone)
    for (size_t i = 0; i < a->bones.size(); i++){
        Vector3 h = W * a->bones[i].head, t = W * a->bones[i].tail;
        float hx, hy, tx, ty;
        if (!vp->ProyectarPunto(h, hx, hy) || !vp->ProyectarPunto(t, tx, ty)) continue;
        float dH = sqrtf((lmx-hx)*(lmx-hx) + (lmy-hy)*(lmy-hy));
        float dT = sqrtf((lmx-tx)*(lmx-tx) + (lmy-ty)*(lmy-ty));
        if (dH < bestPD){ bestPD = dH; bestP = (int)i; bestPMask = 1; }
        if (dT < bestPD){ bestPD = dT; bestP = (int)i; bestPMask = 2; }
        // distancia al SEGMENTO 2D (el cuerpo)
        float dx = tx - hx, dy = ty - hy;
        float len2 = dx * dx + dy * dy;
        float u = (len2 > 1e-6f) ? ((lmx - hx) * dx + (lmy - hy) * dy) / len2 : 0.0f;
        if (u < 0) u = 0; else if (u > 1) u = 1;
        float px = hx + u * dx, py = hy + u * dy;
        float d = sqrtf((lmx - px) * (lmx - px) + (lmy - py) * (lmy - py));
        if (d < bestBD){ bestBD = d; bestB = (int)i; }
    }
    if (bestP >= 0){ if (outMask) *outMask = bestPMask; return bestP; }
    if (outMask) *outMask = 3;
    return bestB;
}

bool BoneEditClick3D(Viewport3D* vp, int mx, int my, int vx, int vy, bool shift){
    Armature* a = BoneEditArm();
    if (!a || !vp) return false;
    if (g_grabConsumioClick){ g_grabConsumioClick = false; return true; } // el click ya confirmo un grab modal
    int mask = 3;
    int b = BonePickEdit(vp, a, (float)(mx - vx), (float)(my - vy), &mask);
    if (!shift) BoneSelLimpiar(a);
    if (b >= 0){
        // shift sobre algo ya seleccionado -> lo saca; si no, seleccion nueva (o agregada)
        bool ya = (mask == 3) ? a->bones[b].select
                : (mask == 1) ? a->bones[b].selHead : a->bones[b].selTail;
        bool nuevo = !(shift && ya);
        if (mask == 3) BoneSelHueso(a, b, nuevo);
        else           BoneSelPunta(a, b, mask, nuevo);
        a->boneActivo = nuevo ? b : -1;
        if (nuevo){
            // arrancar el drag de LO SELECCIONADO (termina al soltar). El pivote de profundidad del
            // arrastre (VelocidadArrastreMundo) va al punto agarrado.
            Matrix4 W; a->GetWorldMatrix(W);
            Vector3 p = (mask == 2) ? a->bones[b].tail
                      : (mask == 1) ? a->bones[b].head
                      : (a->bones[b].head + a->bones[b].tail) * 0.5f;
            TransformPivotPoint = W * p;
            BoneGrabStartSeleccion(a, true);
            g_grabLastX = mx; g_grabLastY = my;
        }
    } else if (!shift) a->boneActivo = -1;
    g_redraw = true;
    return true;
}

// el click que confirma un grab modal (no-porClick) marca esto para que el ScenePick3D del MISMO
// evento no re-pickee (Viewport3D::button_left confirma antes de que llegue el pick).
void BoneGrabMarcarClickConsumido(){ g_grabConsumioClick = true; }

// E de teclado / boton Extrude / menu: extruye desde cada punta seleccionada y deja los TAILS nuevos
// agarrados al mouse (grab MODAL: click o Enter confirman, Esc cancela el movimiento -los huesos
// extruidos quedan-). ejes3d: en el 3D el grab acepta X/Y/Z y muestra la barra de estado.
void BoneEditExtruirInteractivo(Armature* a, bool ejes3d){
    std::vector<int> nuevos;
    if (BoneEditExtruir(a, &nuevos) < 0 || nuevos.empty()) return;
    std::vector<int> masks(nuevos.size(), 2);
    BoneGrabStartEx(a, nuevos, masks, false, 1, ejes3d);
}
// Shift+D interactivo: duplica y deja las copias agarradas (grab modal de huesos enteros)
void BoneEditDuplicarInteractivo(Armature* a, bool ejes3d){
    int nb = BoneEditDuplicar(a);
    if (nb < 0) return;
    std::vector<int> bones, masks;
    if (!BoneSeleccionMasks(a, bones, masks)) return;
    BoneGrabStartEx(a, bones, masks, false, 1, ejes3d);
}
