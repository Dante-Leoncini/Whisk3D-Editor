#ifdef W3D_MCP
#include <typeinfo>  // typeid: el tipo del paso, para la grabadora de pasos (solo PC)
#endif
#include "Undo.h"
extern bool g_redraw;   // el frame se redibuja cuando un undo/redo/borrado cambia algo visible
#include "variables.h"           // InteractionMode, ActiveObj, ObjSelects
#include "objects/Objects.h"     // Object (pos/rot/scale)
#include "objects/Mesh.h"        // Mesh (edit move: vertex/normals/ComputeBorders)
#include "objects/EditMesh.h"    // EditMesh (seleccion de sub-elementos: vertSel/edgeSel/faceSel)
#include "objects/ObjectMode.h" // W3dFreeNameFor: re-chequear el espacio al restaurar un nombre
#include "W3dNames.h"          // W3dUniqueName: el nombre del constraint nuevo, en el espacio del objeto
#include "w3dlog.h"              // w3dLogfW: los re-uniquificados del undo quedan registrados
#include "W3dNotice.h"           // W3dAvisof: la curva huerfana que vuelve en una escena RECREADA se avisa
#include "objects/Light.h"       // Lights (global): el borrado de luces lo des/re-registra
#include "objects/Materials.h"
#include "objects/Textures.h"     // TextureRetain / TextureRelease: las capas de textura del deshacer de un material    // Materials (lista global): destino de rename por INDICE (W3dDestGlobal)
#include "edit/Modifier.h"      // Modifier (limpiar target de Armature/Mirror/Boolean al borrar el objeto apuntado)
#include "io/EditedTexture.h"   // PaintedTextureUndo: los pixeles de una textura pintada
#include "objects/Armature.h"   // Armature (cast correcto Object*<->Armature* al limpiar/restaurar skinArmature)
#include "script/W3dScript.h"   // W3dScriptEntry::refs (destino RefLua de un rename: vive en un vector por valor)
#include "animation/SkeletalAnimation.h" // KeyframesUndo: recorrer las curvas del clip activo (tracks/Propertys)
#include "animation/W3dAnimSet.h"        // clips COMPARTIDOS: restaurar escribe en el clip (copy-on-write); capturar solo lee
#include "animation/Animation.h"         // AnimationObjects / keyFrame / ActiveAnimKind
#include "animation/Armature2DAnimation.h" // clips del armature 2D (kind 4): undo de sus keyframes
#include "animation/VertexAnimation.h"   // KeyframesUndo: snapshot binario de los frames de la vertex anim de objeto
#include "WhiskUI/render/UIOverlay.h"    // UI2D_IsElement2D / Rot2dDe / TamanoElem (campos 2D del transform)
#include "WhiskUI/objects/Text2D.h"      // el texto 2D escala por su 'tam'
#include "WhiskUI/objects/UI.h"          // resize del lienzo (UI::ancho/alto)
#include "objects/MeshResource.h" // el registro de mallas (destino MeshG de un rename) + publicar tras el undo
#include "io/ProjectResources.h"  // lo PURGADO (material / textura en caliente) que el undo vuelve a poner en uso
#include "io/W3dResources.h"      // las bibliotecas de clips de jerarquia que retiene un paso (referencias del historial)
// CameraActive: NO incluyo Camera.h (header pesado del editor, arrastra Target/Curve/icons -> riesgo en el
// build de Symbian). Forward-declaro: solo necesito el puntero (Object es la 1ra base -> el cast a Object* es offset 0).
class Camera; extern Camera* CameraActive;
#include <vector>
#include <set>
#include <map>
#include <string>
#include <stdio.h>   // sprintf GLOBAL (prefijos del remapeo del dope); Symbian/STLport no tiene std::snprintf
// SceneCollection (raiz de la escena) viene de objects/Objects.h

extern void UpdateActiveEditMesh(); // LayoutInput.cpp (refresca g_editMesh al cambiar de modo)
// SELECCION DEL DOPE SHEET (Timeline.cpp): sus claves llevan el INDICE del armature 2D adentro
// ("arm2d:#<serial malla>/a<IDX>/b<n>"), asi que sacar o devolver un armature CORRE esos indices
// igual que corre los del undo. Se declara aca en vez de incluir Timeline.h (header pesado de UI).
extern void DopeRemapIndexKey(const std::string& prefix, int a, int b);
extern void DopeInsertIndexKey(const std::string& prefix, int a);
// el DUENO adentro de esas claves es "#<serial>", NO el nombre (el nombre se recicla: ver el
// bloque de identidad de Timeline.h). Los prefijos de los avisos se arman con esto.
extern std::string DopeIdOwner(const Object* o);
// las filas de VERTEX ANIM se resuelven por la anim ACTIVA -> todo camino que la cambie tiene
// que soltar esa seleccion (ver DopeReleaseVertexAnim en Timeline.h)
extern void DopeReleaseVertexAnim();
extern void InvalidateArmatureSkin(Armature* a); // ObjectMode.cpp (re-skinnea las mallas del rig)

// ============================================================================
//  Comandos de UNDO/REDO. Cada comando guarda UN snapshot del estado y su
//  Apply() INTERCAMBIA el guardado con el vivo (swap). Asi el MISMO Apply()
//  sirve para deshacer (vivo=nuevo -> guardado) y para rehacer (vivo=viejo ->
//  guardado de nuevo): es un toggle. Deshacer pasa el comando del stack de undo
//  al de redo; rehacer al reves. Una accion NUEVA (Push) vacia el stack de redo.
// ============================================================================
class UndoCmd {
public:
    // LA RAIZ del paso (la escena que estaba activa al apilarlo; NULL = no es de una raiz: la biblioteca). Con varias
    // escenas abiertas a la vez el historial es uno solo: antes de aplicarse, la suya pasa a ser la activa
    Object* rootOf;
    UndoCmd() : rootOf(NULL) {}
    virtual ~UndoCmd() {}
    virtual void Apply() = 0; // intercambia el estado guardado con el vivo (undo Y redo)
    // Una lista CORRIO SUS INDICES (se reordeno o se borro un elemento): los miembros que
    // guardan una POSICION en esa lista (un destino de rename, un indice suelto, o un vector
    // PARALELO posicional) tienen que seguir al elemento a su lugar nuevo, sino el proximo
    // Ctrl+Z aplica al elemento equivocado: corrupcion SILENCIOSA (ni crash ni aviso).
    // 'lista' identifica LA LISTA (su .i no se mira); b >= 0 = intercambio a<->b;
    // b < 0 = se BORRO el indice 'a'. Ver UndoMoveLayerMesh / UndoDeletedList.
    //
    // *** ES PURA A PROPOSITO (defensa por construccion) ***
    // Van CINCO tandas de bugs de esta MISMA clase, siempre en un comando que nadie clasifico.
    // Con el default vacio, agregar un comando nuevo que guarda posiciones "compilaba y andaba"
    // hasta que alguien borraba un elemento de abajo. Ahora NO COMPILA hasta que el autor
    // decide una de las dos cosas, mirando la TABLA de clasificacion de miembros de Undo.h:
    //   - tiene miembros de tipo (c) -> escribe un RemapList de verdad;
    //   - no tiene ninguno           -> pone W3D_UNDO_NO_INDICES, que ES la declaracion
    //                                   explicita de "todos mis miembros son (a)/(b)/(d)".
    virtual void RemapList(const W3dRenameDest& list, int a, int b) = 0;
    // 'borrado' se esta DESTRUYENDO de verdad (free): todo objeto que este comando tenga
    // DETACHADO (fuera del arbol) y lo referencie tiene que soltar el puntero. El recorrido
    // del destructor (UnlinkFromTree, Objects.cpp) camina SOLO el arbol vivo y no llega
    // aca. Default vacio: la inmensa mayoria de los comandos no es duena de ningun objeto.
    virtual void UnlinkDetached(Object* deleted) { (void)deleted; }
};
// "revise mis miembros y NINGUNO es una POSICION ni un vector PARALELO a una lista" (tipos
// (a) puntero validado al aplicar / (b) snapshot completo de la lista / (d) valor propio).
// Es una declaracion, no un atajo: si el comando guarda un indice, esto es MENTIRA y vuelve
// el bug silencioso. Poner al lado la clasificacion de cada miembro, como los demas.
#define W3D_UNDO_NO_INDICES  void RemapList(const W3dRenameDest&, int, int) {}

// ============================================================================
//  COMANDO CONTENEDOR: los virtuales se reenvian DESDE UN SOLO LUGAR.
//
//  Hay varios comandos que son "N comandos adentro de uno" (CompositeUndo -el de
//  UndoMergeLast-, MultiRenameUndo, JoinUndo, ApplyUndo, Bone2DRenameUndo). Cada uno
//  reescribia el reenvio a mano, comando por comando y virtual por virtual, y el resultado
//  fue el de siempre: Aplicar y RemapList se reenviaban, pero UnlinkDetached NO. O
//  sea que meter un comando que ES DUENO de memoria (DeleteUndo, ModStackUndo) adentro de un
//  compuesto lo dejaba SIN el aviso de "esto se esta liberando" -> puntero muerto adentro de
//  un contenedor, que es exactamente la familia de bugs que se viene cerrando. Y no era
//  hipotetico: el Apply Modifier arma un compuesto (UndoMergeLast) con un ModStackUndo
//  adentro.
//
//  *** POR QUE ASI Y NO "acordarse de agregar la linea" ***
//  El contenedor NO reimplementa ningun virtual: declara SUS PARTES (Parts()) y esta clase
//  reenvia TODO. Un virtual nuevo en UndoCmd se agrega ACA una sola vez y le llega a los
//  cinco contenedores solos; olvidarse deja de ser posible porque no hay nada que recordar.
//  El unico virtual que un contenedor puede querer redefinir es Aplicar (MultiRenameUndo
//  negocia UN nombre entre todas sus puntas en vez de aplicar una por una).
//
//  Parts() se arma en cada llamada (no se cachea): las partes de un contenedor son fijas
//  desde que se pushea, pero cachear un vector paralelo seria justo otro estado que puede
//  quedar desincronizado.
// ============================================================================
class ContainerUndo : public UndoCmd {
protected:
    // cada contenedor enumera aca sus partes (en el ORDEN en que hay que aplicarlas)
    virtual void Parts(std::vector<UndoCmd*>& out) = 0;
public:
    void Apply() {
        std::vector<UndoCmd*> p; Parts(p);
        for (size_t i = 0; i < p.size(); i++) if (p[i]) p[i]->Apply();
    }
    void RemapList(const W3dRenameDest& L, int a, int b) {
        std::vector<UndoCmd*> p; Parts(p);
        for (size_t i = 0; i < p.size(); i++) if (p[i]) p[i]->RemapList(L, a, b);
    }
    void UnlinkDetached(Object* deleted) {
        std::vector<UndoCmd*> p; Parts(p);
        for (size_t i = 0; i < p.size(); i++) if (p[i]) p[i]->UnlinkDetached(deleted);
    }
};

// cambio de modo Edit/Object
class ModeUndo : public UndoCmd {
    int mode;
public:
    ModeUndo(int m) : mode(m) {}
    void Apply() { int cur = InteractionMode; InteractionMode = mode; mode = cur; UpdateActiveEditMesh(); }
    W3D_UNDO_NO_INDICES // modo: (d) valor propio (Edit/Object), no indexa ninguna lista
};

// ============================================================================
//  IDENTIDAD ESTABLE DEL DESTINO DE UN RENAME (ver el bloque W3dRenameDest en Undo.h).
//  Guardar un std::string* crudo a un nombre que vive DENTRO de un vector POR VALOR
//  (huesos 3D/2D, mesh parts, refs de lua) es un use-after-free esperando: la
//  proxima alta/baja realoca el vector. Se guarda DUENO + INDICE y se resuelve
//  ACA, al aplicar; si el dueno murio o el indice ya no existe -> NULL (no-op).
//  Y el caso Directo (los duenos "del heap") tampoco se devuelve a ciegas: esos
//  duenos TAMBIEN se borran (las capas de la malla se liberan y se recrean, el
//  boton "-" borra un grupo, un material/clip/objeto se borra), asi que el puntero
//  se busca POR IDENTIDAD entre los nombres vivos antes de escribir en el.
// ============================================================================
// el objeto sigue colgando de la escena? (Delete lo DETACHA sin liberar: mientras
// esta afuera, su rename no aplica; el undo del borrado corre antes -por LIFO- y
// lo devuelve al arbol, asi que la secuencia normal sigue funcionando igual)
static bool ObjectInScene(Object* o) {
    if (!o || !SceneCollection) return false;
    std::vector<Object*> stack; stack.push_back(SceneCollection);
    while (!stack.empty()) {
        Object* n = stack.back(); stack.pop_back();
        if (n == o) return true;
        for (size_t i = 0; i < n->Childrens.size(); i++) stack.push_back(n->Childrens[i]);
    }
    return false;
}
W3dRenameDest W3dDestName(std::string* p) {
    // RED DE SEGURIDAD: 'Directo' se resuelve con W3dPointerNameLive, que SOLO reconoce
    // &Object::name y las puntas de vinculo que enumera Object::RefObjectName (targetName,
    // RailName y la fuente de cada constraint). Un puntero a cualquier otro nombre (un
    // UVGroup::nombre, un W3dBone::name...) no matchea NUNCA -> el resolver devuelve NULL
    // siempre y el paso de undo es un no-op MUDO. Eso rompio el binding del rig 2D en silencio.
    // Las APIs de captura ya no aceptan std::string* pelado (rompe en compilacion), asi que
    // esto solo puede pasar si alguien construye el destino a mano: que quede en el log.
    if (p && !W3dPointerNameLive(p))
        w3dLogfW("[undo] W3dDestNombre con un puntero que NO es un nombre de Object: "
                 "el paso de rename va a ser un no-op (usa W3dDestCapaMalla/Hueso3D/... )");
    W3dRenameDest d; d.type = W3dRenameDest::Direct; d.ptr = p; return d;
}
W3dRenameDest W3dDestBone3D(Armature* a, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::Bone3D; d.owner = (void*)a; d.i = idx; return d;
}
W3dRenameDest W3dDestBone2D(Mesh* m, Armature2D* arm, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::Bone2D; d.owner = (void*)m; d.owner2 = (void*)arm; d.i = idx; return d;
}
W3dRenameDest W3dDestMeshPart(Mesh* m, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::MeshPart; d.owner = (void*)m; d.i = idx; return d;
}
W3dRenameDest W3dDestRefLua(Object* o, int script, int ref) {
    W3dRenameDest d; d.type = W3dRenameDest::RefLua; d.owner = (void*)o; d.i = script; d.j = ref; return d;
}
W3dRenameDest W3dDestLayerMesh(Mesh* m, int layer, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::LayerMesh; d.owner = (void*)m; d.which = layer; d.i = idx; return d;
}
W3dRenameDest W3dDestClipArm(Armature* a, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::ClipArm; d.owner = (void*)a; d.i = idx; return d;
}
W3dRenameDest W3dDestClip2D(Mesh* m, Armature2D* arm, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::Clip2D; d.owner = (void*)m; d.owner2 = (void*)arm; d.i = idx; return d;
}
W3dRenameDest W3dDestGlobal(int list, int idx) {
    W3dRenameDest d; d.type = W3dRenameDest::Global; d.which = list; d.i = idx; return d;
}
// LOS DOS DESTINOS APUNTAN A LA MISMA LISTA? (el indice NO se mira: identifica al contenedor,
// no al elemento). Es lo que usa el remapeo para saber a que destinos les corrio el indice:
// una lista es (tipo + dueno + dueno2 + cual). Los tipos que no tienen alguno de esos campos
// lo dejan en 0 en TODOS sus destinos, asi que comparar los cuatro siempre alcanza.
static bool W3dSameList(const W3dRenameDest& d, const W3dRenameDest& list) {
    return d.type == list.type && d.owner == list.owner &&
           d.owner2 == list.owner2 && d.which == list.which;
}
// remapea UN indice guardado, con la misma convencion que RemapList: b >= 0 = intercambio
// a<->b; b < 0 = se BORRO el indice 'a'. Devuelve false si el indice guardado apuntaba JUSTO
// al elemento borrado (el que lo guardo decide que hacer: los destinos de rename mueren, los
// pasos de reordenar tambien, un vector paralelo pierde ESA entrada).
// Vive aca arriba porque lo usan comandos de todo el archivo (MeshGeoUndo es el primero).
static bool RemapIndexSaved(int& idx, int a, int b) {
    if (b >= 0) {
        if      (idx == a) idx = b;
        else if (idx == b) idx = a;
        return true;
    }
    if (idx == a) return false;   // era ESE elemento
    if (idx >  a) idx--;          // los de arriba del borrado bajan uno
    return true;
}
// el armature 2D sigue colgando de la malla? (mismo criterio que Bones2DUndo::Vive)
static bool Arm2DInMesh(Mesh* m, Armature2D* arm) {
    if (!m || !arm) return false;
    for (size_t k = 0; k < m->armatures2d.size(); k++) if (m->armatures2d[k] == arm) return true;
    return false;
}
std::string* W3dDestResolve(const W3dRenameDest& d) {
    switch (d.type) {
        case W3dRenameDest::Direct:
            // Directo = un nombre que es MIEMBRO de un Object (Object::name / targetName /
            // RailName): no se recrea, pero el OBJETO se puede borrar (y liberar al caerse del
            // stack de undo). Se chequea que el puntero siga siendo uno de esos miembros en un
            // objeto de la escena; si no, NULL -> no-op seguro en vez de escribir en un colgado.
            if (!d.ptr || !W3dPointerNameLive(d.ptr)) return NULL;
            return d.ptr;
        case W3dRenameDest::Bone3D: {
            Armature* a = (Armature*)d.owner;
            if (!a || !ObjectInScene(a)) return NULL;
            if (d.i < 0 || d.i >= (int)a->bones.size()) return NULL;
            return &a->bones[d.i].name;
        }
        case W3dRenameDest::Bone2D: {
            Mesh* m = (Mesh*)d.owner; Armature2D* arm = (Armature2D*)d.owner2;
            if (!m || !ObjectInScene(m) || !Arm2DInMesh(m, arm)) return NULL;
            if (d.i < 0 || d.i >= (int)arm->bones.size()) return NULL;
            return &arm->bones[d.i].name;
        }
        case W3dRenameDest::MeshPart: {
            Mesh* m = (Mesh*)d.owner;
            if (!m || !ObjectInScene(m)) return NULL;
            if (d.i < 0 || d.i >= (int)m->materialsGroup.size()) return NULL;
            return &m->materialsGroup[d.i].name;
        }
        case W3dRenameDest::RefLua: {
            Object* o = (Object*)d.owner;
            if (!o || !ObjectInScene(o) || !o->scriptData) return NULL;
            if (d.i < 0 || d.i >= (int)o->scriptData->scripts.size()) return NULL;
            std::vector<std::pair<std::string, std::string> >& refs = o->scriptData->scripts[d.i].refs;
            if (d.j < 0 || d.j >= (int)refs.size()) return NULL;
            return &refs[d.j].second;
        }
        // CAPAS de la malla: vertex groups / UV groups / UV maps / capas de color / armatures 2D /
        // vertex anims. Todas son vector<T*> y el elemento SE LIBERA: Mesh::FreeLayers (o sea
        // CUALQUIER op de geometria, via MeshGeoUndo::ApplyTo) las destruye y las rehace con new,
        // y el boton "-" de las tarjetas borra la activa. Por eso van por (malla, lista, indice).
        case W3dRenameDest::LayerMesh: {
            Mesh* m = (Mesh*)d.owner;
            if (!m || !ObjectInScene(m)) return NULL;
            switch (d.which) {
                case W3dRenameDest::VGroup:
                    if (d.i < 0 || d.i >= (int)m->vertexGroups.size() || !m->vertexGroups[d.i]) return NULL;
                    return &m->vertexGroups[d.i]->name;
                case W3dRenameDest::UVGroup:
                    if (d.i < 0 || d.i >= (int)m->uvGroups.size() || !m->uvGroups[d.i]) return NULL;
                    return &m->uvGroups[d.i]->name;
                case W3dRenameDest::UVMap:
                    if (d.i < 0 || d.i >= (int)m->uvMaps.size() || !m->uvMaps[d.i]) return NULL;
                    return &m->uvMaps[d.i]->name;
                case W3dRenameDest::ColorLayer:
                    if (d.i < 0 || d.i >= (int)m->colorLayers.size() || !m->colorLayers[d.i]) return NULL;
                    return &m->colorLayers[d.i]->name;
                case W3dRenameDest::Arm2D:
                    if (d.i < 0 || d.i >= (int)m->armatures2d.size() || !m->armatures2d[d.i]) return NULL;
                    return &m->armatures2d[d.i]->name;
                case W3dRenameDest::VertAnim:
                    if (d.i < 0 || d.i >= (int)m->animations.size() || !m->animations[d.i]) return NULL;
                    return &m->animations[d.i]->name;
            }
            return NULL;
        }
        case W3dRenameDest::ClipArm: {   // clip 3D: Armature::animations (se borra con el "-" de la tarjeta)
            Armature* a = (Armature*)d.owner;
            if (!a || !ObjectInScene(a)) return NULL;
            if (d.i < 0 || d.i >= (int)a->animations.size() || !a->animations[d.i]) return NULL;
            W3dOwnArmatureAnims(a);   // se va a escribir el nombre: nunca en un clip compartido
            return &a->animations[d.i]->name;
        }
        case W3dRenameDest::Clip2D: {    // clip del armature 2D: Armature2D::anims
            Mesh* m = (Mesh*)d.owner; Armature2D* arm = (Armature2D*)d.owner2;
            if (!m || !ObjectInScene(m) || !Arm2DInMesh(m, arm)) return NULL;
            if (d.i < 0 || d.i >= (int)arm->anims.size() || !arm->anims[d.i]) return NULL;
            return &arm->anims[d.i]->name;
        }
        case W3dRenameDest::Global: {    // listas del PROYECTO (no cuelgan de ningun objeto)
            if (d.which == W3dRenameDest::MaterialG) {
                if (d.i < 0 || d.i >= (int)Materials.size() || !Materials[d.i]) return NULL;
                return &Materials[d.i]->name;
            }
            if (d.which == W3dRenameDest::SceneAnimG) {
                if (d.i < 0 || d.i >= (int)SceneAnimations.size() || !SceneAnimations[d.i]) return NULL;
                return &SceneAnimations[d.i]->name;
            }
            // el registro de mallas SE PURGA (W3dMeshesPurgeUnused): la malla va por su serial
            if (d.which == W3dRenameDest::MeshG || d.which == W3dRenameDest::MeshFolderG) {
                MeshResource* r = W3dMeshResourceBySerial(d.i);
                if (!r) return NULL;
                return (d.which == W3dRenameDest::MeshG) ? &r->name : &r->folder;
            }
            return NULL;
        }
    }
    return NULL;
}
// mismo destino? (para descartar repetidos al capturar varias puntas de un vinculo)
static bool SameDest(const W3dRenameDest& a, const W3dRenameDest& b) {
    if (a.type != b.type) return false;
    if (a.type == W3dRenameDest::Direct) return a.ptr == b.ptr;
    return a.owner == b.owner && a.owner2 == b.owner2 && a.which == b.which && a.i == b.i && a.j == b.j;
}

// ============================================================================
//  RENAME (objeto/material/uv/color/hueso/...): el destino es un W3dRenameDest, que
//  se resuelve al APLICAR (arriba). El nombre GUARDADO puede haber sido TOMADO por
//  otro mientras el paso estaba en el stack, asi que al restaurarlo hay que
//  RE-CHEQUEAR el espacio de nombres (fallo F2):
//     "add cube / objname Zzz / add cube / undo"  ->  dos 'Cubo.001' vivos.
//  W3dFreeNameFor (ObjectMode.cpp) ubica el espacio del puntero por IDENTIDAD y
//  devuelve el siguiente libre; si el puntero NO es un nombre (una ref de lua, un
//  targetName) lo devuelve tal cual: esos son el OTRO extremo de un vinculo y tienen
//  que volver al valor exacto, no uniquificarse.
// ============================================================================
class RenameUndo : public UndoCmd {
    W3dRenameDest dest;
    std::string   saved;
public:
    // (el DUENO del nombre que se LIBERA de verdad -el streaming descarga lo generado-: el destino muere, no-op seguro;
    //  un Directo ya se valida por W3dPointerNameLive)
    void UnlinkDetached(Object* deleted) { if (dest.owner == (void*)deleted) { dest.owner = NULL; dest.owner2 = NULL; } }
    // NO hay ctor desde std::string*: el destino SIEMPRE es un W3dRenameDest (ver Undo.h).
    RenameUndo(const W3dRenameDest& D) : dest(D) {
        std::string* p = W3dDestResolve(dest);
        if (p) saved = *p;
    }
    const std::string& Saved() const { return saved; }
    std::string* Destination() const { return W3dDestResolve(dest); } // se resuelve RECIEN AHORA
    void Apply() { ApplyWith(NULL); }
    // 'forzar' != NULL: escribe ESE valor (ya negociado por MultiRenameUndo entre todas
    // las puntas del vinculo) en vez de re-uniquificar por su cuenta.
    void ApplyWith(const std::string* force) {
        std::string* t = W3dDestResolve(dest);
        if (!t) return;   // el dueno (o el elemento) ya no existe: no-op SEGURO
        std::string cur = *t;
        std::string returns = saved;
        if (force) returns = *force;
        else {
            const char* label = NULL;
            returns = W3dFreeNameFor(t, saved, &label);
            if (returns != saved)
                w3dLogfW("[nombres] undo: '%s' ya estaba tomado (%s) -> vuelve como '%s'",
                         saved.c_str(), label ? label : "?", returns.c_str());
        }
        *t = returns;
        saved = cur;
        // el nombre de una capa / grupo / mesh part es dato de la MALLA: si es de un RECURSO
        // compartido, el Ctrl+Z tambien le llega a sus otros objetos (el tick lo publica)
        if (dest.type == W3dRenameDest::LayerMesh || dest.type == W3dRenameDest::MeshPart)
            W3dMeshMarkEdited((Mesh*)dest.owner);
    }
    // la lista corrio sus indices: el destino sigue a SU elemento (ver UndoMoveLayerMesh /
    // UndoDeletedList). b >= 0 = swap a<->b; b < 0 = se borro el indice 'a' (el destino de
    // ESE elemento muere -no-op seguro- y los de arriba bajan uno).
    void RemapList(const W3dRenameDest& list, int a, int b) {
        if (!W3dSameList(dest, list)) return;
        if (b >= 0) {
            if      (dest.i == a) dest.i = b;
            else if (dest.i == b) dest.i = a;
        }
        else if (dest.i == a) dest.i = -1;   // el dueno del nombre ya no existe
        else if (dest.i >  a) dest.i--;
    }
};

// N comandos cualesquiera en UN SOLO paso (ver UndoMergeLast en Undo.h)
class CompositeUndo : public ContainerUndo {
    std::vector<UndoCmd*> parts;
    // SECUENCIAL (UndoGroupSequentialEnd): cada parte se hizo SOBRE el resultado de la anterior
    // (la malla se hizo unica y DESPUES se horneo el transform en ella). Deshacer va AL REVES y
    // rehacer AL DERECHO. Con el mismo orden las dos veces, el vinculo volvia primero y el paso de
    // geometria guardaba como "estado de redo" la geometria de ANTES: el Ctrl+Y la perdia.
    // El compuesto de siempre (partes independientes, o que dependen de ir en ESE orden en los dos
    // sentidos: la mudanza del outliner y los nombres) no cambia.
    bool sequential;
    bool undone;   // (secuencial) el proximo Aplicar es el redo
protected:
    void Parts(std::vector<UndoCmd*>& out){ out = parts; }   // el reenvio de los virtuales lo hace ContainerUndo
public:
    CompositeUndo(bool sec = false) : sequential(sec), undone(false) {}
    ~CompositeUndo(){ for (size_t i = 0; i < parts.size(); i++) delete parts[i]; }
    void Add(UndoCmd* c){ if (c) parts.push_back(c); }
    void Apply() {
        if (!sequential) { ContainerUndo::Apply(); return; }
        if (!undone) { for (size_t i = parts.size(); i-- > 0; ) if (parts[i]) parts[i]->Apply(); }
        else           { for (size_t i = 0; i < parts.size(); i++) if (parts[i]) parts[i]->Apply(); }
        undone = !undone;
    }
};

// ============================================================================
//  RENAME MULTIPLE en UN SOLO paso (ver UndoCaptureRenames en Undo.h): todas las
//  puntas de UN vinculo por nombre (vertex group + hueso 3D, uv group + hueso 2D,
//  objeto + refs de lua + targetName/RailName). Todas guardan EL MISMO nombre viejo.
//  Al restaurar, si ese nombre se lo tomo otro (fallo F2) NO alcanza con uniquificar
//  cada punta por separado: cada espacio daria un sufijo distinto y el vinculo quedaria
//  ROTO (justo lo que este comando existe para evitar). Se negocia UN nombre libre en
//  TODOS los espacios involucrados y se escribe el mismo en todas.
// ============================================================================
class MultiRenameUndo : public ContainerUndo {
    std::vector<RenameUndo*> parts;
protected:
    void Parts(std::vector<UndoCmd*>& out){ for (size_t i = 0; i < parts.size(); i++) out.push_back(parts[i]); }
public:
    ~MultiRenameUndo(){ for (size_t i = 0; i < parts.size(); i++) delete parts[i]; }
    void Add(const W3dRenameDest& d){ if (W3dDestResolve(d)) parts.push_back(new RenameUndo(d)); }
    bool IsEmpty() const { return parts.empty(); }
    void Apply(){
        if (parts.empty()) return;
        const std::string request = parts[0]->Saved();
        // negociacion: se pide 'pedido' a cada espacio; el que lo rechaza propone otro y
        // se vuelve a preguntar a todos. W3dFreeNameFor siempre avanza el sufijo, asi
        // que converge en pocas vueltas (el tope es una red de seguridad).
        std::string cand = request;
        for (int attempt = 0; attempt < 64; attempt++) {
            std::string prop = cand;
            for (size_t i = 0; i < parts.size(); i++) {
                if (parts[i]->Saved() != request) continue;   // no es punta de ESTE nombre
                std::string free = W3dFreeNameFor(parts[i]->Destination(), prop, NULL);
                if (free != prop) prop = free;
            }
            if (prop == cand) break;
            cand = prop;
        }
        if (cand != request)
            w3dLogfW("[nombres] undo: '%s' ya estaba tomado -> las %d punta(s) del vinculo vuelven como '%s'",
                     request.c_str(), (int)parts.size(), cand.c_str());
        for (size_t i = 0; i < parts.size(); i++) {
            if (parts[i]->Saved() == request) parts[i]->ApplyWith(&cand);
            else                                 parts[i]->Apply();   // otro nombre: por su cuenta
        }
    }
};

// seleccion de objetos: la lista seleccionada + el activo
class SelectUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) {
        for (size_t i = sel.size(); i-- > 0; ) if (sel[i] == deleted) sel.erase(sel.begin() + (long)i);
        if (active == deleted) active = NULL;
    }
private:
    std::vector<Object*> sel;
    Object* active;
public:
    SelectUndo() { sel = ObjSelects; active = ActiveObj; }
    void Apply() {
        std::vector<Object*> curSel = ObjSelects; Object* curAct = ActiveObj; // estado vivo
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = false;
        ObjSelects = sel; ActiveObj = active;
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = true;
        sel = curSel; active = curAct; // guarda lo que estaba vivo (para rehacer)
    }
    W3D_UNDO_NO_INDICES // sel/activo: (a) punteros a Object (el arbol de escena no se indexa)
};

// VISIBILIDAD de un objeto en el EDITOR (tecla 'h'): guarda el estado PREVIO de 'visible' y hace SWAP al
// aplicar (el mismo Apply() sirve para deshacer Y rehacer). Mismo criterio de lifetime que SelectUndo
// (puntero a Object; el arbol de escena no se indexa). Antes 'h' toggleaba sin registrar undo -> Ctrl+Z no lo
// revertia (reporte del dueno). Espeja ChangeVisibilityObj (Objects.cpp): si queda oculto y es/contiene luz, la apaga.
class VisibilityUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if (obj == deleted) obj = NULL; }
private:
    Object* obj;
    bool    visible;   // estado GUARDADO (para el swap)
    bool    renderable;   // (la camarita del outliner: si sale en el render; va en el mismo paso)
public:
    VisibilityUndo(Object* o) : obj(o), visible(o ? o->visible : true), renderable(o ? o->renderable : true) {}
    void Apply() {
        if (!obj) return;
        bool cur = obj->visible;
        obj->visible = visible;
        visible = cur;                        // guarda lo vivo (para rehacer)
        cur = obj->renderable; obj->renderable = renderable; renderable = cur;
        extern bool w3dRenderLights;          // fwd-decl: evita el header pesado de gfx en el build de Symbian
        if (!obj->visible && w3dRenderLights) TurnOffChildLights(obj);
    }
    W3D_UNDO_NO_INDICES // obj: puntero a Object; visible / renderable: valores propios -> no indexa ninguna lista
};

// transform en OBJECT MODE: pos/rot/escala de los seleccionados al EMPEZAR
// rotEuler va aparte del quaternion: el quaternion no distingue 0 de 360, asi que sin guardarlo el undo de un
// giro de vuelta entera te devolvia la orientacion pero te comia las vueltas (y con ellas la animacion).
// Los ELEMENTOS 2D guardan ADEMAS sus campos propios (rot2d / ancho / alto o el tam del
// texto, y el ancho/alto del UI): los G/R/S y handles del Editor 2D tocan ESO, no scale.
struct TEst { Object* o; Vector3 pos; Quaternion rot; Vector3 rotEuler; Vector3 scale;
              bool es2d; float rot2d, w2, h2; };
class TransformUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { for (size_t i = 0; i < e.size(); i++) if (e[i].o == deleted) e[i].o = NULL; }
private:
    std::vector<TEst> e;
    // que campos 2D tiene 'o' y sus valores actuales (rot2d solo en Element2D)
    static bool Read2D(Object* o, float* r, float* w, float* h) {
        *r = 0.0f; *w = 0.0f; *h = 0.0f;
        if (UI2D_IsElement2D(o)) {
            *r = *UI2D_Rot2dOf(o);
            float *ew, *eh;
            if (UI2D_ElemSize(o, &ew, &eh)) { *w = *ew; *h = *eh; }
            else *w = ((Text2D*)o)->size;
            return true;
        }
        if (o->getType() == ObjectType::ui) {
            *w = ((UI*)o)->width; *h = ((UI*)o)->height;
            return true;
        }
        return false;
    }
    static void Write2D(Object* o, float r, float w, float h) {
        if (UI2D_IsElement2D(o)) {
            *UI2D_Rot2dOf(o) = r;
            float *ew, *eh;
            if (UI2D_ElemSize(o, &ew, &eh)) { *ew = w; *eh = h; }
            else ((Text2D*)o)->size = w;
        } else if (o->getType() == ObjectType::ui) {
            ((UI*)o)->width = w; ((UI*)o)->height = h;
        }
    }
public:
    TransformUndo() {
        for (size_t i = 0; i < ObjSelects.size(); i++) Capture(ObjSelects[i]);
        // proportional editing: los NO seleccionados que vienen arrastrados (ya estan en el snapshot del transform)
        for (size_t i = 0; i < objectState.size(); i++)
            if (objectState[i].distProp >= 0.0f && objectState[i].obj) Capture(objectState[i].obj);
    }
    TransformUndo(Object* only) { Capture(only); }   // un objeto puntual (resize del lienzo)
    void Capture(Object* o) {
        if (!o) return;
        TEst t; t.o = o; t.pos = o->pos; t.rot = o->Rot(); t.rotEuler = o->rotEuler; t.scale = o->scale;
        t.es2d = Read2D(o, &t.rot2d, &t.w2, &t.h2);
        e.push_back(t);
    }
    bool IsEmpty() const { return e.empty(); }
    // hubo cambio real? Un drag que no se movio (un click) no merece un paso de undo.
    bool Differs() const {
        for (size_t i = 0; i < e.size(); i++) {
            Object* o = e[i].o; if (!o) continue;
            if (o->pos.x != e[i].pos.x || o->pos.y != e[i].pos.y || o->pos.z != e[i].pos.z) return true;
            if (o->rotEuler.x != e[i].rotEuler.x || o->rotEuler.y != e[i].rotEuler.y ||
                o->rotEuler.z != e[i].rotEuler.z) return true;
            if (o->scale.x != e[i].scale.x || o->scale.y != e[i].scale.y || o->scale.z != e[i].scale.z) return true;
            if (e[i].es2d) {
                float r, w, h; Read2D(o, &r, &w, &h);
                if (r != e[i].rot2d || w != e[i].w2 || h != e[i].h2) return true;
            }
        }
        return false;
    }
    void Apply() {
        for (size_t i = 0; i < e.size(); i++) {
            Object* o = e[i].o; if (!o) continue;
            Vector3 cp = o->pos; Quaternion cr = o->Rot(); Vector3 ce = o->rotEuler; Vector3 cs = o->scale; // vivo
            o->pos = e[i].pos; o->scale = e[i].scale;
            o->SetRotSnapshot(e[i].rot, e[i].rotEuler);    // tal cual se capturo (con sus vueltas)
            e[i].pos = cp; e[i].rot = cr; e[i].rotEuler = ce; e[i].scale = cs; // guarda lo vivo
            if (e[i].es2d) {
                float r, w, h; Read2D(o, &r, &w, &h);           // vivo 2D
                Write2D(o, e[i].rot2d, e[i].w2, e[i].h2);
                e[i].rot2d = r; e[i].w2 = w; e[i].h2 = h;
            }
        }
    }
    W3D_UNDO_NO_INDICES // e[]: (a) TEst.o es un Object* + (d) sus pos/rot/escala. No es paralelo
                         // a ninguna lista: cada entrada LLEVA su puntero, no una posicion.
};

// mover verts/aristas/caras en EDIT MODE (move PURO): intercambia las posiciones+normales de la malla.
class EditMoveUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    // POSICIONES + ESTADO DE POSE en un solo valor (W3dPosVerts, ver Mesh.h): restaurar
    // posiciones es restaurar tambien EN QUE ESPACIO estan (modelo o pose de un cuadro).
    W3dPosVerts vpos;
    std::vector<GLbyte>  normals;
public:
    EditMoveUndo(Mesh* M) : m(M) {
        if (m) vpos.Capture(m);
        if (m && m->normals && m->vertexSize > 0) normals.assign(m->normals, m->normals + m->vertexSize * 3);
    }
    bool IsEmpty() const { return vpos.pos.empty(); }
    void Apply() {
        if (!m) return;
        {
            // deshacer/rehacer un move TAMBIEN mueve posiciones: las claves de sharp/seam
            // son los BYTES de la posicion, asi que sin el guard el Ctrl+Z de un move
            // dejaba las marcas ancladas donde ya no hay ningun vertice (antes: se perdian
            // en silencio al guardar; ahora ademas ComputeBorders las podaria). El guard
            // CIERRA ANTES del ComputeBorders de abajo: primero re-anclar, despues podar.
            W3dMoveVerts mv(m);
            vpos.SwapWith(m);   // <- posiciones Y estado de pose, en el mismo swap
            m->UninstantiateData(W3DMD_NOR);   // COW: las normales se intercambian en el lugar
            if (m->normals && (int)normals.size() == m->vertexSize * 3) for (size_t i = 0; i < normals.size(); i++) { GLbyte  c = m->normals[i]; m->normals[i] = normals[i]; normals[i] = c; }
        }
        // move PURO = NO cambia la topologia -> ComputeBorders(false) CONSERVA la edit mesh (no la rebuildea)
        // asi NO se pierde la SELECCION; SyncPos re-lee las posiciones restauradas al display del edit.
        m->ComputeBorders(false);
        if (m->edit) m->edit->SyncPos();
        W3dMeshMarkEdited(m);   // la malla de un RECURSO: el Ctrl+Z lo ven todos sus objetos
    }
    W3D_UNDO_NO_INDICES // m: (a) Mesh*; vertex/normals: (b) snapshot completo de los arrays
};

// SELECCION de sub-elementos en EDIT MODE (verts/edges/faces): intercambia los 3 vectores de seleccion
// + el activo de la EditMesh (solo si el size matchea -> robusto al rearmado de la edit).
// Y LO OCULTO (H / Shift+H / Alt+H, EditMesh::Hide / Reveal): ocultar cambia la seleccion Y lo que se ve, asi que va en el
// mismo paso. Antes Ctrl+Z no traia de vuelta lo ocultado (ni volvia a ocultar lo revelado con Alt+H)
class SelectEditUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    std::vector<unsigned char> vs, is, fs;
    std::vector<unsigned char> vh, eh, fh;   // lo oculto (vacio = nada oculto todavia: los vectores se arman al ocultar)
    int active;
    // lo oculto se intercambia solo si los TRES calzan con la jaula de ahora (vacios o del tamano justo)
    static bool Fits(const std::vector<unsigned char>& a, int n) { return a.empty() || (int)a.size() == n; }
public:
    SelectEditUndo(Mesh* M) : m(M), active(-1) {
        if (m) { m->EnsureEdit();
                 if (m->edit) { vs = m->edit->vertSel; is = m->edit->edgeSel; fs = m->edit->faceSel; active = m->edit->activeIdx;
                                if (m->edit->numHidden > 0) { vh = m->edit->vertHid; eh = m->edit->edgeHid; fh = m->edit->faceHid; } } }
    }
    void Apply() {
        if (!m) return; m->EnsureEdit(); if (!m->edit) return;
        EditMesh* e = m->edit;
        if (e->vertSel.size() == vs.size()) e->vertSel.swap(vs);
        if (e->edgeSel.size() == is.size()) e->edgeSel.swap(is);
        if (e->faceSel.size() == fs.size()) e->faceSel.swap(fs);
        int cur = e->activeIdx; e->activeIdx = active; active = cur;
        const int nV = e->NumVerts(), nE = e->NumEdges(), nF = e->NumFaces();
        // (sin nada oculto ni antes ni ahora no hay nada que tocar: el click de siempre)
        const bool hiddenNow = e->numHidden > 0, hiddenSaved = !vh.empty() || !eh.empty() || !fh.empty();
        if ((hiddenNow || hiddenSaved) && Fits(e->vertHid, nV) && Fits(e->edgeHid, nE) && Fits(e->faceHid, nF) &&
            Fits(vh, nV) && Fits(eh, nE) && Fits(fh, nF)) {
            std::vector<unsigned char> v2, e2, f2;   // lo de ahora (para rehacer); vacio si no habia nada oculto
            if (hiddenNow) { v2 = e->vertHid; e2 = e->edgeHid; f2 = e->faceHid; }
            e->vertHid = vh; e->edgeHid = eh; e->faceHid = fh;
            vh.swap(v2); eh.swap(e2); fh.swap(f2);
            e->numHidden = 0;
            for (size_t k = 0; k < e->vertHid.size(); k++) e->numHidden += e->vertHid[k];
            for (size_t k = 0; k < e->edgeHid.size(); k++) e->numHidden += e->edgeHid[k];
            for (size_t k = 0; k < e->faceHid.size(); k++) e->numHidden += e->faceHid[k];
            e->RebuildVisible();   // (lo que dibuja la malla: sin lo oculto)
            if (!m->modifiers.empty()) m->GenerateMeshModified();   // (y el resultado de los modificadores)
        }
        e->Recolor(); // re-tinta los buffers segun la seleccion restaurada
    }
    // vs/es/fs: (b) snapshot completo de los 3 vectores de seleccion (con guard de tamano).
    // 'activo' es (c) una posicion en la EDIT MESH, pero esa no es una lista con nombre ni
    // tiene destinos de rename ni botones de reordenar/borrar: se rearma entera de la geo.
    W3D_UNDO_NO_INDICES
};

// cambiar el MATERIAL de un mesh part (ActionPickedMaterial): intercambia el Material* del mesh part
class MaterialUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m; int idx; Material* saved;
public:
    MaterialUndo(Mesh* M, int i) : m(M), idx(i),
        saved((M && i >= 0 && i < (int)M->materialsGroup.size()) ? M->materialsGroup[i].material : NULL) {}
    void Apply() {
        if (m && idx >= 0 && idx < (int)m->materialsGroup.size()) {
            Material* cur = m->materialsGroup[idx].material;
            m->materialsGroup[idx].material = saved; saved = cur;
            // el material es dato de la MALLA: si es un recurso compartido vuelve en todos sus objetos
            if (m->mesh && idx < (int)m->mesh->parts.size())
                W3dMeshResourceChangeMaterial(m->mesh, idx, m->materialsGroup[idx].material);
        }
    }
    // 'idx' es (c): POSICION en Mesh::materialsGroup (la lista de los mesh parts). Hoy esa
    // lista es familia (1) -reordenar/borrar un mesh part empuja UndoCaptureMeshGeo, que la
    // restaura entera, y por LIFO los indices vuelven solos-, asi que NADIE la remapea. Igual
    // se implementa: es gratis y deja el comando correcto el dia que aparezca un borrado de
    // mesh part que no sea deshacible (ahi solo hay que agregar el UndoDeletedList).
    void RemapList(const W3dRenameDest& list, int a, int b) {
        if (!W3dSameList(W3dDestMeshPart(m, -1), list)) return;
        if (!RemapIndexSaved(idx, a, b)) idx = -1;   // borraron ESE mesh part: no-op seguro
    }
};

// snapshot COMPLETO de la geometria de la malla (topologia): extrude / delete-edit / loop-cut / duplicate /
// assign mesh part. Clona TODOS los arrays de render + faces3d + materialsGroup + las capas (uv/color/grupos).
// Apply() = SWAP: snapshotea la geo viva, escribe la guardada, y se queda con la que estaba viva (redo).
class MeshGeoUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    int vertexSize, facesSize;
    // POSICIONES + ESTADO DE POSE en un solo valor (W3dPosVerts, ver Mesh.h). Antes esto era
    // un std::vector<GLfloat> pelado y el flag Mesh::posedByAnim NO se snapshoteaba: deshacer
    // una op de topologia hecha en un cuadro POSADO, despues de volver al cuadro base, escribia
    // el vertex[] POSADO con el flag apagado y el ComputeBorders de ApplyTo podaba TODAS las
    // marcas contra la pose, callado. Guardadas juntas no hay nada que recordar.
    W3dPosVerts           vpos;
    std::vector<GLfloat>  uv;
    std::vector<GLbyte>   normals;
    std::vector<GLubyte>  color;
    std::vector<MeshIndex> faces;
    std::vector<MeshFace> faces3d;
    std::vector<int>      looseEdges;
    std::vector<int>      looseVerts;
    std::vector<MaterialGroup> materialsGroup;
    std::vector<UVMap>       uvMaps;       int uvActiveMap;
    std::vector<ColorLayer>  colorLayers;  int activeColor;
    std::vector<VertexGroup> vertexGroups; int activeGroup;
    // UV GROUPS (pesos por CORNER): sus indices son RENDER-VERTS, o sea que una op de topologia
    // los re-mapea (GenerateRender) -> viajan con la geometria, como vertCtrlPoint y las anims.
    std::vector<UVGroup>     uvGroups;     int uvActiveGroup;
    // rest del skinning 2D: es PAREJO a uv[] (invariante uv = f(uv2dRest, pose), ver Mesh.h). Si
    // no viajara con la geo, deshacer una op de topologia dejaba un rest de otro layout.
    std::vector<GLfloat>     uv2dRest;
    std::vector< std::vector<GLfloat> > uvExtra; // el render de las capas UV extra (parejo a uv[])
    std::vector<int>         vertCtrlPoint; int skinNCtrl; // SKINNING: mapeo render-vert -> control-point (sino el undo de una malla skinneada deja el mapeo viejo -> skin roto)
    std::set<std::string>    sharpEdges, seamEdges; // bordes sharp/seam (por POSICION). meshSmooth = shading
    std::vector<int>         posRep;     // agrupamiento por posicion de la JAULA: dos verts coincidentes que estaban separados (pre-merge) siguen separados al deshacer
    bool                     meshSmooth;
    // FRAMES de las VERTEX ANIMS: el remap de topologia (RemapVertexAnims via GenerateRender)
    // los reescribe al layout nuevo. Sin snapshot, deshacer una op de topologia dejaba la
    // geometria VIEJA con frames del layout NUEVO -> anim muerta (guard de vcount) o, peor,
    // el proximo remap los leia desalineados y la malla animada se rompia. Viajan con la geo.
    VtxAnimSnap              vanims;
    // ---- y ACA vivia el fallo (c) de este comando --------------------------------------
    // vanims.anims es un vector PARALELO POSICIONAL a Mesh::animations, y VertexAnimRestoreSnap
    // (Core) lo restaura POR POSICION. Pero Mesh::animations es familia (2): el "-" de la tarjeta
    // borra una vertex anim SIN empujar ningun paso de undo -> los indices se corren y este
    // snapshot escribia los frames de cada anim ENCIMA de la de abajo (silencioso: el unico guard
    // era "misma cantidad de keyframes"). NO se puede arreglar snapshoteando la LISTA entera
    // (opcion (i) de Undo.h): eso resucitaria la anim borrada, y el borrado NO es deshacible.
    // Se guarda entonces, para cada slot del snapshot, EL INDICE VIVO al que pertenece, y se
    // remapea como cualquier otro indice (-1 = su anim se borro -> ese slot no se restaura).
    std::vector<int>         vanimIdx;   // vanimIdx[k] = indice en Mesh::animations del slot k
    void CaptureIdxVanims() {
        vanimIdx.clear();
        for (size_t k = 0; k < vanims.anims.size(); k++) vanimIdx.push_back((int)k);
    }
    // escribe el snapshot de frames en la malla RESPETANDO el remapeo (slot k -> anim vanimIdx[k]).
    // Se arma un VtxAnimSnap temporal ya ORDENADO como la lista viva y se reusa el restaurador del
    // Core tal cual (los slots sin snapshot quedan con vcount 0 = "no tocar"). Los buffers se
    // MUEVEN con swap (sin copiar los frames) y se devuelven al terminar.
    void RestoreVanims(Mesh* s) {
        if (!vanims.has) return;
        VtxAnimSnap tmp; tmp.has = true;
        tmp.anims.resize(s->animations.size());
        std::vector<int> placed(vanims.anims.size(), -1);
        for (size_t k = 0; k < vanims.anims.size(); k++) {
            const int d = (k < vanimIdx.size()) ? vanimIdx[k] : (int)k;
            if (d < 0 || d >= (int)tmp.anims.size()) continue;   // su anim se borro (o ya no existe)
            tmp.anims[d].vcount = vanims.anims[k].vcount;
            tmp.anims[d].frames.swap(vanims.anims[k].frames);
            placed[k] = d;
        }
        VertexAnimRestoreSnap(s, tmp);
        for (size_t k = 0; k < vanims.anims.size(); k++)          // devolver los buffers prestados
            if (placed[k] >= 0) vanims.anims[k].frames.swap(tmp.anims[placed[k]].frames);
    }

    void CaptureFrom(Mesh* s) { // llena los miembros desde la malla viva
        // la edicion de una malla de un RECURSO puede estar pendiente (MeshResource.h): el
        // snapshot tiene que llevar las caras y capas de verdad, no las listas vacias
        if (!s->pendingEdit.empty()) W3dMeshBinMaterializeEdit(s);
        vertexSize = s->vertexSize; facesSize = s->facesSize;
        VertexAnimSnapshot(s, vanims); // los frames de las vertex anims, parejos a ESTA geometria
        CaptureIdxVanims();            // recien capturado: cada slot esta en su lugar (identidad)
        sharpEdges = s->sharpEdges; seamEdges = s->seamEdges; meshSmooth = s->meshSmooth; posRep = s->posRep;
        normals.clear(); uv.clear(); color.clear(); faces.clear();
        uvMaps.clear(); colorLayers.clear(); vertexGroups.clear(); uvGroups.clear();
        uv2dRest = s->uv2dRest;
        uvExtra = s->uvExtra;
        vpos.Capture(s);   // posiciones Y estado de pose (la unica puerta; ver Mesh.h)
        if (s->normals)     normals.assign(s->normals, s->normals + vertexSize * 3);
        if (s->uv)          uv.assign(s->uv, s->uv + vertexSize * 2);
        if (s->vertexColor) color.assign(s->vertexColor, s->vertexColor + vertexSize * 4);
        if (s->faces)       faces.assign(s->faces, s->faces + facesSize);
        faces3d = s->faces3d; looseEdges = s->looseEdges; looseVerts = s->looseVerts; materialsGroup = s->materialsGroup;
        for (size_t i = 0; i < s->uvMaps.size(); i++)       uvMaps.push_back(*s->uvMaps[i]);
        for (size_t i = 0; i < s->colorLayers.size(); i++)  colorLayers.push_back(*s->colorLayers[i]);
        for (size_t i = 0; i < s->vertexGroups.size(); i++) vertexGroups.push_back(*s->vertexGroups[i]);
        for (size_t i = 0; i < s->uvGroups.size(); i++)     uvGroups.push_back(*s->uvGroups[i]);
        uvActiveMap = s->uvActiveMap; activeColor = s->activeColor; activeGroup = s->activeGroup;
        uvActiveGroup = s->uvActiveGroup;
        vertCtrlPoint = s->vertCtrlPoint; skinNCtrl = s->skinNCtrl; // skinning: mapeo render->control-point
    }
    void ApplyTo(Mesh* s) { // escribe los miembros (snapshot) a la malla viva
        // las capas COMPARTIDAS (un recurso, un .obj) se abandonan sin liberarlas: se reemplazan enteras
        s->UninstantiateData(W3DMD_ALL, false);
        s->pendingEdit.clear();   // la edicion que se escribe abajo es la de verdad
        delete[] s->normals;     s->normals = NULL;
        delete[] s->uv;          s->uv = NULL;
        delete[] s->vertexColor; s->vertexColor = NULL;
        delete[] s->faces;       s->faces = NULL;
        s->vertexSize = vertexSize; s->facesSize = facesSize;
        vpos.WriteIn(s);   // vertex[] Y posedByAnim: el ComputeBorders del final poda con el gate CORRECTO
        if (!normals.empty()) { s->normals = new GLbyte[normals.size()];       for (size_t i=0;i<normals.size();i++) s->normals[i]     = normals[i]; }
        if (!uv.empty())      { s->uv = new GLfloat[uv.size()];                for (size_t i=0;i<uv.size();i++)      s->uv[i]          = uv[i]; }
        if (!color.empty())   { s->vertexColor = new GLubyte[color.size()];    for (size_t i=0;i<color.size();i++)   s->vertexColor[i] = color[i]; }
        if (!faces.empty())   { s->faces = new MeshIndex[faces.size()];         for (size_t i=0;i<faces.size();i++)   s->faces[i]       = faces[i]; }
        s->faces3d = faces3d; s->looseEdges = looseEdges; s->looseVerts = looseVerts; s->materialsGroup = materialsGroup;
        s->sharpEdges = sharpEdges; s->seamEdges = seamEdges; s->meshSmooth = meshSmooth; s->posRep = posRep;
        s->FreeLayers();
        for (size_t i = 0; i < uvMaps.size(); i++)       s->uvMaps.push_back(new UVMap(uvMaps[i]));
        for (size_t i = 0; i < colorLayers.size(); i++)  s->colorLayers.push_back(new ColorLayer(colorLayers[i]));
        for (size_t i = 0; i < vertexGroups.size(); i++) s->vertexGroups.push_back(new VertexGroup(vertexGroups[i]));
        for (size_t i = 0; i < uvGroups.size(); i++)     s->uvGroups.push_back(new UVGroup(uvGroups[i]));
        s->uvActiveMap = uvActiveMap; s->activeColor = activeColor; s->activeGroup = activeGroup;
        s->uvActiveGroup = uvActiveGroup; s->uv2dRest = uv2dRest;
        s->uvExtra = uvExtra;   // (FreeLayers de arriba lo vacio)
        s->vertCtrlPoint = vertCtrlPoint; s->skinNCtrl = skinNCtrl; // restaurar el mapeo render->control-point (skinning)
        RestoreVanims(s); // los frames de las vertex anims vuelven JUNTO con su geometria (mismo layout), CADA UNO A SU ANIM
        s->lastSkinFrame = -999999; // forzar re-skin con la geo/mapeo restaurados (ComputeBorders ya bumpea skinGeomVersion)
        // invalida el edit (se rearma de la geo restaurada) pero CONSERVA el posRep del snapshot: reagrupar por
        // posicion soldaba en la jaula dos verts coincidentes (el des-merge de un auto merge) y la jaula quedaba con
        // un vert de menos -> la seleccion guardada por indice ya no calzaba y el move siguiente la dejaba vieja
        s->ComputeBorders(true, false);
    }
public:
    // SELECCION del edit mesh al momento del snapshot (posiciones de los verts seleccionados). Restaurar la
    // geometria invalida la jaula y Construir la rearma con TODO seleccionado: sin esto cada Ctrl+Z de un
    // merge/loop cut/extrude dejaba "todo seleccionado" y el usuario perdia el hilo de los pasos.
    // Se guarda POR INDICE (exacto: distingue dos verts coincidentes, como el des-merge de un auto merge) y
    // POR POSICION (respaldo si la jaula rearmada no tiene la misma cantidad de verts).
    std::vector<Vector3> selPos; bool selCaptured;
    std::vector<unsigned char> selV, selE, selF; int activeSel;
    void CaptureSel(Mesh* s) {
        selPos.clear(); selV.clear(); selE.clear(); selF.clear(); activeSel = -1; selCaptured = false;
        if (!s || !s->edit) return;
        const EditMesh* e = s->edit; selCaptured = true;
        selV = e->vertSel; selE = e->edgeSel; selF = e->faceSel; activeSel = e->activeIdx;
        for (size_t k = 0; k < e->vertSel.size() && k*3+2 < e->pos.size(); k++)
            if (e->vertSel[k]) selPos.push_back(Vector3(e->pos[k*3], e->pos[k*3+1], e->pos[k*3+2]));
    }
    void RestoreSel(Mesh* s) {
        s->EnsureEdit(); EditMesh* e = s->edit; if (!e) return;
        if (e->vertSel.size() == selV.size() && e->edgeSel.size() == selE.size() && e->faceSel.size() == selF.size()) {
            e->vertSel = selV; e->edgeSel = selE; e->faceSel = selF; e->activeIdx = activeSel; e->Recolor();
        } else s->RebuildEditSelByPos(selPos);
    }
    MeshGeoUndo(Mesh* M) : m(M), vertexSize(0), facesSize(0), uvActiveMap(0), activeColor(0), activeGroup(0),
                           uvActiveGroup(-1), skinNCtrl(0), selCaptured(false), activeSel(-1) {
        if (m) { CaptureFrom(m); CaptureSel(m); }
    }
    void Apply() {
        if (!m) return;
        MeshGeoUndo cur(m);   // snapshotea la geo VIVA (la nueva)
        ApplyTo(m);           // escribe la geo GUARDADA (la vieja) a la malla
        // queda con lo que estaba vivo (para rehacer): intercambia los buffers con cur
        vertexSize = cur.vertexSize; facesSize = cur.facesSize;
        vpos.Swap(cur.vpos); // posiciones + estado de pose (para rehacer), juntos
        normals.swap(cur.normals); uv.swap(cur.uv); color.swap(cur.color);
        faces.swap(cur.faces); faces3d.swap(cur.faces3d); looseEdges.swap(cur.looseEdges); looseVerts.swap(cur.looseVerts);
        materialsGroup.swap(cur.materialsGroup);
        uvMaps.swap(cur.uvMaps); colorLayers.swap(cur.colorLayers); vertexGroups.swap(cur.vertexGroups);
        uvGroups.swap(cur.uvGroups); uv2dRest.swap(cur.uv2dRest);
        vertCtrlPoint.swap(cur.vertCtrlPoint); skinNCtrl = cur.skinNCtrl; // (faltaban: el redo escribia el mapeo VIEJO -> skin roto)
        uvActiveMap = cur.uvActiveMap; activeColor = cur.activeColor; activeGroup = cur.activeGroup;
        uvActiveGroup = cur.uvActiveGroup;
        sharpEdges.swap(cur.sharpEdges); seamEdges.swap(cur.seamEdges); meshSmooth = cur.meshSmooth; posRep.swap(cur.posRep);
        vanims.anims.swap(cur.vanims.anims); vanims.has = cur.vanims.has; // los frames de las anims (para rehacer)
        vanimIdx.swap(cur.vanimIdx);  // el snapshot nuevo salio de la lista VIVA -> sus indices son la identidad
        // la seleccion que habia en ESE paso (en Edit Mode la jaula se rearma ya con ella)
        if (selCaptured && InteractionMode == EditMode) RestoreSel(m);
        selPos.swap(cur.selPos); selV.swap(cur.selV); selE.swap(cur.selE); selF.swap(cur.selF);
        { const int ta = activeSel; activeSel = cur.activeSel; cur.activeSel = ta; }
        { const bool t = selCaptured; selCaptured = cur.selCaptured; cur.selCaptured = t; }
        // la geometria cambio -> refrescar el preview del modificador (subdivision/screw). Antes esto lo hacia el
        // regen POR FRAME de UpdateActiveEditMesh; ahora que ese esta gateado, hay que pedirlo aca explicito.
        if (!m->modifiers.empty()) m->GenerateMeshModified();
        // la malla de un RECURSO: el Ctrl+Z es del recurso (lo ven todos sus usuarios), se publica
        // cuando sale de edicion (o ya, si el undo pasa en Modo Objeto)
        W3dMeshMarkEdited(m);
    }
    // (c) vanimIdx / vanims: vector PARALELO a Mesh::animations (vertex anims), familia (2)
    //     -> se remapea aca (ver el bloque de arriba).
    // (b) el resto (arrays de render, faces3d, materialsGroup, uvMaps/colorLayers/vertexGroups/
    //     uvGroups, uv2dRest, vertCtrlPoint...) es snapshot COMPLETO: ApplyTo reemplaza las
    //     listas enteras, asi que sus indices internos (uvActiveMap, activeGroup...) vuelven
    //     coherentes con la lista que restauran. Por eso esas cuatro capas son familia (1).
    // (a) m: Mesh* (los callers lo validan; el comando muere con la escena).
    void RemapList(const W3dRenameDest& list, int a, int b) {
        if (!W3dSameList(W3dDestLayerMesh(m, W3dRenameDest::VertAnim, -1), list)) return;
        for (size_t k = 0; k < vanimIdx.size(); k++)
            if (!RemapIndexSaved(vanimIdx[k], a, b)) vanimIdx[k] = -1; // borraron ESA anim
    }
};

// COLOR (RGBA) de un material/luz: intercambia los 4 floats del target. Lo usa el ColorPicker al cerrar.
// el material sigue VIVO? Los materiales del proyecto son globales y hoy NADIE los borra (Materials nunca
// hace erase y ~Material esta vacio), pero el chequeo es de una linea y es lo que le pide el criterio de
// Undo.h a un miembro (a): "puntero validado al aplicar". El material POR DEFECTO no entra a la lista
// (ver el ctor en Materials.cpp) y tambien es un destino legitimo del panel.
static bool MaterialLive(const Material* m) {
    if (!m) return false;
    if (m == MaterialDefault) return true;
    for (size_t i = 0; i < Materials.size(); i++) if (Materials[i] == m) return true;
    return false;
}
class ColorUndo : public UndoCmd {
    GLfloat* t; GLfloat val[4];
public:
    ColorUndo(GLfloat* target, const GLfloat* old) : t(target) { for (int i=0;i<4;i++) val[i]=old[i]; }
    void Apply() { if (!t) return; for (int i=0;i<4;i++) { GLfloat c=t[i]; t[i]=val[i]; val[i]=c; } }
    // t: (a) puntero al RGBA de un MATERIAL o de una LUZ; val[4]: (d) valores propios.
    // No se puede revalidar un GLfloat* pelado (no dice de quien es), asi que POR QUE ES SEGURO:
    //  - materiales: no se borran nunca (ver MaterialLive);
    //  - luces: son Objects y SI se borran, pero el unico que los LIBERA es el destructor de un
    //    DeleteUndo con inScene==false, y ese comando esta SIEMPRE MAS ARRIBA en g_undo que este
    //    (se pusheo despues). El desalojo por MAXU saca el del FRENTE, o sea el MAS VIEJO: este
    //    ColorUndo se destruye ANTES que el DeleteUndo que libera su luz. Y por LIFO, para llegar a
    //    aplicarlo hay que deshacer primero el borrado, que devuelve la luz a la escena.
    //  Si algun dia se puede borrar un material, o si un comando pasa a liberar objetos sin ser el
    //  tope del stack, esto necesita un destino tipado (como W3dRenameDest) en vez de un float*.
    W3D_UNDO_NO_INDICES
};

// MODIFICACION de un material (checkboxes + shininess; los COLORES van por ColorUndo). Snapshot SOLO de
// esos campos (NO name/texture/capas -> esos tienen su propio undo y no hay que revertirlos de mas).
class MaterialModUndo : public UndoCmd {
    // cuantos flags copian Leer/Escribir (textureOn..chrome). Los buffers Y los loops usan ESTA
    // constante: antes el miembro era bool[10] y los loops iban hasta 11 -> el de Apply() ESCRIBIA
    // b[10], o sea FUERA del arreglo, pisando el miembro de al lado (rmode), y Differs() comparaba
    // el bool basura de cb[10] (daba "cambio" un material que no cambio). GCC lo cantaba con
    // -Waggressive-loop-optimizations. Si se agrega un flag: sumarlo en Leer/Escribir y aca.
    static const int NFLAGS = 12;   // [10] = lineas (aristas por material), [11] = perPixel (sombreado por pixel)
    Material* mat;
    bool b[NFLAGS]; int rmode; float shin; // rmode = reflectMode (era el bool chromeEquirect, ahora int de 3 modos)
    float glinea;                          // thicknessLine (px): viaja con el flag lineas
    float acorte;                          // alphaTest (recorte por alfa, 0 = apagado): fila "Alpha Test" del panel
    float mezb;                            // mixB (mezcla de texturas, 0 = solo la base): fila "Texture mix"
    // LOS MAPAS: numero / textura y la capa UV de cada uno, sus numeros (rugosidad, metalico, alfa) y el IOR
    // (+ la fuerza de la emision: su fila de la tarjeta)
    struct Maps { bool ct[W3D_MAPS]; int uv[W3D_MAPS]; float rug, met, alpha, ior; float emK;
        void Read(const Material* m) { for (int k = 0; k < W3D_MAPS; k++) { ct[k] = m->mapWithTexture[k]; uv[k] = m->uvMap[k]; }
                                       rug = m->rtRoughness; met = m->rtMetallic; alpha = m->diffuse[3]; ior = m->ior;
                                       emK = m->emissionStrength; }
        void Write(Material* m) const { for (int k = 0; k < W3D_MAPS; k++) { m->mapWithTexture[k] = ct[k]; m->uvMap[k] = uv[k]; }
                                           m->rtRoughness = rug; m->rtMetallic = met; m->diffuse[3] = alpha; m->ior = ior;
                                           m->emissionStrength = emK; }
        bool Equal(const Maps& o) const { for (int k = 0; k < W3D_MAPS; k++) if (ct[k] != o.ct[k] || uv[k] != o.uv[k]) return false;
                                           return rug == o.rug && met == o.met && alpha == o.alpha && ior == o.ior && emK == o.emK; } };
    Maps maps;
    // LAS CAPAS DE TEXTURA (borrar, agregar, subir/bajar, la mezcla, la UV, el factor, prendida, la luz horneada): una copia
    // de la lista con su PROPIA referencia de cada textura (asi no se libera mientras el paso exista). Aplicar
    // INTERCAMBIA la lista viva con la guardada: cada una sigue duena de sus referencias
    std::vector<TexLayer> layers;
    static bool LayersEqual(const std::vector<TexLayer>& a, const std::vector<TexLayer>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].tex != b[i].tex || a[i].blend != b[i].blend || a[i].uvMap != b[i].uvMap || a[i].on != b[i].on ||
                a[i].force != b[i].force || a[i].baked != b[i].baked) return false;
        return true;
    }
    static void Read(Material* s, bool* bo, int& rm, float& sh, float& gl, float& ac, float& mb) {
        bo[0]=s->textureOn; bo[1]=s->filtered; bo[2]=s->transparent; bo[3]=s->vertexColor;
        bo[4]=s->lighting; bo[5]=s->repeat; bo[6]=s->uv8bit; bo[7]=s->culling;
        bo[8]=s->depth_test; bo[9]=s->chrome; bo[10]=s->lines; bo[11]=s->perPixel;
        rm=s->reflectMode; sh=s->shininess; gl=s->thicknessLine; ac=s->alphaTest; mb=s->mixB;
    }
    static void Write(Material* s, const bool* bo, int rm, float sh, float gl, float ac, float mb) {
        s->textureOn=bo[0]; s->filtered=bo[1]; s->transparent=bo[2]; s->vertexColor=bo[3];
        s->lighting=bo[4]; s->repeat=bo[5]; s->uv8bit=bo[6]; s->culling=bo[7];
        s->depth_test=bo[8]; s->chrome=bo[9]; s->lines=bo[10]; s->perPixel=bo[11];
        s->reflectMode=rm; s->shininess=sh; s->thicknessLine=gl; s->alphaTest=ac; s->mixB=mb;
    }
public:
    MaterialModUndo(Material* M) : mat(M), rmode(0), shin(0), glinea(1.0f), acorte(0.0f), mezb(0.0f) {
        if (mat) {
            Read(mat, b, rmode, shin, glinea, acorte, mezb); maps.Read(mat);
            layers = mat->layers;
            for (size_t i = 0; i < layers.size(); i++) if (layers[i].tex) TextureRetain(layers[i].tex);
        }
    }
    ~MaterialModUndo() { for (size_t i = 0; i < layers.size(); i++) if (layers[i].tex) TextureRelease(layers[i].tex); }
    Material* Mat() const { return mat; }
    bool Differs() const {
        if (!MaterialLive(mat)) return false;
        bool cb[NFLAGS]; int crm; GLfloat cs, cg, ca, cm; Read(mat, cb, crm, cs, cg, ca, cm);
        for (int i=0;i<NFLAGS;i++) if (cb[i]!=b[i]) return true;
        Maps cmp; cmp.Read(mat); if (!cmp.Equal(maps)) return true;
        if (!LayersEqual(mat->layers, layers)) return true;
        return crm != rmode || cs != shin || cg != glinea || ca != acorte || cm != mezb;
    }
    void Apply() {
        if (!MaterialLive(mat)) return;   // (a): el puntero se REVALIDA al aplicar, no se usa a ciegas
        bool cb[NFLAGS]; int crm; GLfloat cs, cg, ca, cm; Read(mat, cb, crm, cs, cg, ca, cm);   // estado vivo
        Maps live; live.Read(mat);
        Write(mat, b, rmode, shin, glinea, acorte, mezb);                                   // restaura el guardado
        maps.Write(mat);
        for (int i=0;i<NFLAGS;i++) b[i]=cb[i]; rmode=crm; shin=cs; glinea=cg; acorte=ca; mezb=cm; // guarda lo vivo (para rehacer)
        maps = live;
        mat->layers.swap(layers);   // (las capas: la guardada vuelve, la viva queda para rehacer)
    }
    // b[NFLAGS] es un arreglo FIJO de flags del material, NO un vector paralelo a una lista.
    W3D_UNDO_NO_INDICES // mat: (a) Material* REVALIDADO al aplicar (MaterialLive); b/rmode/shin/glinea/acorte: (d) valores propios
};

// ============================================================================
//  BORRAR objetos (Ctrl+Z): los objetos NO se liberan al borrar -> se DETACHAN de la escena (se sacan de su
//  padre) y los GUARDA el comando, que los re-inserta al deshacer. El comando es DUENO mientras estan
//  detachados; si se cae del stack (destructor con inScene=false) recien ahi los libera de verdad. Maneja
//  las LUCES (global Lights: las saca/agrega para que una luz borrada NO siga iluminando) y la CAMARA ACTIVA.
// ============================================================================
static void DetachLights(Object* o) { // saca del global Lights todas las luces del subarbol (dejan de iluminar)
    if (o->getType() == ObjectType::light)
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { Lights.erase(Lights.begin()+i); break; }
    for (size_t i = 0; i < o->Childrens.size(); i++) DetachLights(o->Childrens[i]);
}
static void ReattachLights(Object* o) { // re-registra las luces del subarbol (sin duplicar)
    if (o->getType() == ObjectType::light) {
        Light* l = (Light*)o; bool already = false;
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == l) { already = true; break; }
        if (!already) Lights.push_back(l);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) ReattachLights(o->Childrens[i]);
}
static bool ContainsCamActive(Object* o) {
    if (CameraActive && (Object*)CameraActive == o) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (ContainsCamActive(o->Childrens[i])) return true;
    return false;
}
// el ESQUELETO cuyo clip esta activo (ActiveAnimArm/kind): si se borra, la seleccion de animacion queda colgada
// (el timeline seguia mostrando el clip borrado). Al detacharlo se vuelve a ESCENA (kind 0). El destructor del
// Armature tambien lo hace, pero el borrado DETACHA (no destruye, por el undo) -> hay que resetearlo aca.
extern int ActiveAnimKind; extern Armature* ActiveAnimArm; extern Mesh* ActiveAnimMesh;
static bool ContainsAnimActive(Object* o) {
    if (ActiveAnimArm && (Object*)ActiveAnimArm == o) return true;
    if (ActiveAnimMesh && (Object*)ActiveAnimMesh == o) return true;   // vertex anim (kind 3): la malla en edicion
    for (size_t i = 0; i < o->Childrens.size(); i++) if (ContainsAnimActive(o->Childrens[i])) return true;
    return false;
}

// 'mio' = el comando SACO de verdad este objeto del arbol y por lo tanto es su DUENO. NO es
// decorativo: es lo unico que separa "lo tengo yo, detachado" de "sigue vivo en la escena", y
// el destructor libera SOLO lo primero. Ver el paso 0 de Detachar.
struct DelEntry { Object* obj; Object* parent; int index; bool mine;
                  DelEntry() : obj(0), parent(0), index(0), mine(false) {} };
// recolecta los "delete-roots": cada objeto SELECCIONADO y borrable (no-collection salvo incCol) cuyo
// ancestro NO es tambien un delete-root. El subarbol se va con su root (no se recursea adentro).
static void CollectDelete(Object* node, bool incCol, std::vector<DelEntry>& out) {
    for (int i = (int)node->Childrens.size()-1; i >= 0; i--) { // height->under: los indices quedan validos al sacar
        Object* c = node->Childrens[i];
        bool deletable = c->select && (incCol || c->getType() != ObjectType::collection);
        if (deletable) { DelEntry e; e.obj = c; e.parent = node; e.index = i; out.push_back(e); }
        else          CollectDelete(c, incCol, out);
    }
}

// hijo que se REPARENTA al abuelo cuando se borra su padre (asi no desaparece de la escena; queda como hijo del
// abuelo preservando su posicion global, v1: solo traslacion, igual que W3dReparent).
// 'movida' = ESTE comando fue el que la movio (y por lo tanto es el que tiene que devolverla).
// Sin esa marca, el replay reponia a ciegas y podia meter el MISMO Object* en DOS Childrens
// (ver el bloque "EL HIJO PRESERVADO PUDO MUDARSE" en Detachar/Aplicar).
struct RepEntry { Object* child; Object* grandparent; Object* deleted; Vector3 posUnderGrandparent; Vector3 posUnderDeleted;
                  bool moved; RepEntry() : child(0), grandparent(0), deleted(0), moved(false) {} };

// ============================================================================
//  REFERENCIA POR PUNTERO a un objeto que se BORRA. Hay que LIMPIARLA al borrar, sino queda
//  COLGANDO: el que referencia sigue usando al borrado (la malla sigue deformando contra un
//  esqueleto borrado -"borre el esqueleto y el modelo sigue con la animacion"-, la
//  instancia sigue DIBUJANDO un objeto que el outliner ya no lista) y, cuando el comando de undo
//  se cae del stack y hace el delete de verdad, es un puntero MUERTO: crash, o -peor- la
//  direccion se recicla y la referencia cae sobre OTRO objeto en silencio. Se restaura en el undo.
//
//  DOS FAMILIAS, mismo tratamiento:
//   - slot < 0: refs de la MALLA. mod != NULL = era mod->target (Armature/Mirror/Boolean);
//     mod == NULL = era skinArmature. Van SOLO por puntero (no tienen vinculo por nombre).
//   - slot >= 0 y withSerial == 0: refs PROPIAS de una clase, enumeradas por el Core
//     (Object::OwnRefs/OwnRef, ver Objects.h): toda la familia Target -Instance, Mirror
//     objeto, Camera y ObjectScript- mas el RIEL de la camara. Estas SI tienen un vinculo
//     por NOMBRE al lado
//     (targetName / RailName) y hay que llevarselo junto con el puntero, sino el .w3d guardado
//     queda con "target": "<objeto que no esta en el archivo>" (corrupcion muda: al reabrir el
//     vinculo se pierde sin un solo aviso). Es la MISMA conducta que ya tenia el modificador,
//     que al perder su target se guarda sin campo "target".
//   - slot >= 0 y withSerial != 0: la fuente de un CONSTRAINT del objeto. La puerta les da un slot
//     a cada uno DESPUES de las refs propias de la clase, pero esa lista SE REORDENA Y SE BORRA
//     (Add/Remove/Move Up/Down), asi que el slot no alcanza para volver a encontrar la entrada:
//     se guarda ademas el SERIAL y se resuelve por ahi, igual que mod/modSerial.
// ============================================================================
struct RefEntry {
    Object* owner;      // el que referencia (la Mesh si slot < 0)
    Modifier* mod;      // solo slot < 0
    unsigned int modSerial;  // identidad ESTABLE de ese modificador (ver LiveModIn abajo)
    unsigned int withSerial; // != 0: la ref es la fuente del constraint con ESE serial (el slot se ignora)
    int which;               // con withSerial: CUAL puntero del constraint (0 = la fuente/target, 1 = el pole del IK)
    int slot;         // -1 = ref de malla; >= 0 = indice de Object::RefObject
    Object* target;   // a quien apuntaba
    std::string name; // solo slot >= 0: el vinculo por nombre que acompania al puntero
};
// ============================================================================
//  EL MODIFICADOR SIGUE VIVO Y ES EL MISMO? (no-op seguro de RefWrite)
//
//  'mod' es el unico puntero de este comando a algo cuya vida el comando NO controla: el
//  stack de modificadores es del Mesh y sus altas/bajas son otro paso de undo (ver
//  ModStackUndo). Escribir 'mod->target' a ciegas era la FAMILIA DE SIEMPRE: si el
//  modificador se libero y el allocator reciclo la direccion, el undo escribia el target
//  ENCIMA del modificador de OTRA malla y eso se GUARDABA en el .w3d ("tipo": "mirror",
//  "target": "Base" en una malla que nunca tuvo target). Corrupcion muda, persistida.
//  Repro y control: test 'modtargetuaf' (main/test/W3dScript.cpp), fase B.
//
//  Se chequean LAS DOS COSAS y hacen falta las dos:
//   - que el puntero siga estando en el stack de SU malla (cubre el free "para siempre"), y
//   - que el SERIAL coincida (W3dModSerial, edit/Modifier.h): la direccion reciclada por un
//     modificador nuevo DE LA MISMA MALLA pasa el chequeo de arriba pero no este.
//  Si no da, es un NO-OP: no escribe en ningun lado (nunca "en cualquier lado").
// ============================================================================
static bool LiveModIn(Mesh* m, Modifier* mod, unsigned int serial){
    if (!m || !mod) return false;
    for (size_t i = 0; i < m->modifiers.size(); i++)
        if (m->modifiers[i] == mod) return m->modifiers[i]->serial.v == serial;
    return false;
}
static bool InSubtree(Object* root, Object* found){
    if (!root) return false;
    if (root == found) return true;
    for (size_t i = 0; i < root->Childrens.size(); i++) if (InSubtree(root->Childrens[i], found)) return true;
    return false;
}
static bool PointsToDeleted(Object* target, const std::vector<DelEntry>& ents){
    if (!target) return false;
    for (size_t i = 0; i < ents.size(); i++) if (InSubtree(ents[i].obj, target)) return true;
    return false;
}
// recorre la escena y junta TODA referencia por puntero que apunte a un objeto que se borra:
// las de la malla (modificador / skinArmature) y las que el Core enumera solo (familia Target
// + riel de camara). Nadie se acuerda de sumar su clase aca: la enumeracion generica hace que
// una clase nueva con un Object* entre sola.
static void CollectRefs(Object* node, const std::vector<DelEntry>& ents, std::vector<RefEntry>& out){
    if (!node) return;
    if (node->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)node;
        for (size_t i = 0; i < m->modifiers.size(); i++){ Modifier* md = m->modifiers[i];
            if (md && PointsToDeleted(md->target, ents)){
                RefEntry r; r.owner=m; r.mod=md; r.modSerial=md->serial.v; r.withSerial=0; r.slot=-1; r.target=md->target; out.push_back(r); } }
        if (PointsToDeleted((Object*)m->skinArmature, ents)){
            RefEntry r; r.owner=m; r.mod=NULL; r.modSerial=0; r.withSerial=0; r.slot=-1; r.target=(Object*)m->skinArmature; out.push_back(r); }
    }
    const int own = node->OwnRefs();   // donde empieza el bloque de constraints en la puerta
    const int nref = node->RefsObject();
    for (int i = 0; i < nref; i++){
        Object* t = node->RefObject(i);
        if (!PointsToDeleted(t, ents)) continue;
        RefEntry r; r.owner=node; r.mod=NULL; r.modSerial=0; r.withSerial=0; r.which=0; r.slot=i; r.target=t;
        // del bloque de constraints para arriba, el slot es una POSICION en una lista que se reordena
        // y se borra: la identidad de la entrada es el serial (ver RefWrite). Lo mismo los slots PROPIOS de
        // un armature, que son el target y el pole de los constraints de sus HUESOS (W3dConstraintAtRef).
        (void)own;
        { int w = 0; W3dConstraint* c = W3dConstraintAtRef(node, i, &w);
          if (c) { r.withSerial = c->serial.v; r.which = w; } }
        std::string* name = node->RefObjectName(i); if (name) r.name = *name;
        out.push_back(r);
    }
    for (size_t i = 0; i < node->Childrens.size(); i++) CollectRefs(node->Childrens[i], ents, out);
}
// escribe una ref: restaurar=false la SUELTA (detachar/redo), restaurar=true devuelve lo guardado (undo)
static void RefWrite(RefEntry& r, bool restore){
    if (r.slot < 0){
        Mesh* m = (Mesh*)r.owner;
        // el modificador puede haberse ido (y su direccion reciclada) entre la captura y esto:
        // si no es EL MISMO, no se escribe nada (ver LiveModIn arriba, test 'modtargetuaf').
        if (r.mod) { if (LiveModIn(m, r.mod, r.modSerial)) r.mod->target = restore ? r.target : NULL; }
        else if (m) m->skinArmature = restore ? (Armature*)r.target : NULL;
        if (m) {
            m->lastSkinFrame = -999999;
            // el target cambio (se fue o volvio): la malla generada quedo VIEJA. Sin esto un Boolean seguia
            // mostrando el corte de un cilindro ya borrado hasta que algo la regeneraba. Se regenera ACA (y no
            // "en el proximo frame"): el frame solo regenera el objeto activo, y el dueno del modificador
            // normalmente no lo es cuando borras su target.
            m->GenerateMeshModified();
            g_redraw = true;
        }
        return;
    }
    if (!r.owner) return;
    if (r.withSerial){
        // la fuente de un CONSTRAINT: se busca por SERIAL y se IGNORA el slot. Entre la captura y
        // esto el usuario pudo agregar, borrar o mover entradas del stack, y entonces el slot apunta
        // a OTRO constraint: escribirlo ahi es la misma corrupcion muda que el 'mod' reciclado. Si el
        // constraint ya no existe no se escribe en ningun lado (patron LiveModIn, test 'modtargetuaf').
        W3dConstraint* c = W3dConstraintBySerialAny(r.owner, r.withSerial);   // (de objeto o de hueso)
        if (!c) return;
        W3dConstraintSetRef(c, r.which, restore ? r.target : NULL, restore ? r.name : std::string());
        if (r.owner->getType() == ObjectType::armature) ((Armature*)r.owner)->poseDirty = true;
        return;
    }
    r.owner->SetRefObject(r.slot, restore ? r.target : NULL);
    std::string* name = r.owner->RefObjectName(r.slot);
    if (name) *name = restore ? r.name : std::string();
}

// re-uniquifica el nombre de 'o' y de todo su subarbol en el scope donde acaba de
// quedar. PRE-ORDEN (el orden de FindObjectByName). Solo escribe si hizo falta.
static void ReuniquifySubtree(Object* o) {
    if (!o) return;
    const std::string old = o->name;
    o->SetNameObj(old);
    if (o->name != old)
        w3dLogfW("[nombres] undo del borrado: '%s' ya estaba tomado -> vuelve como '%s'",
                 old.c_str(), o->name.c_str());
    for (size_t i = 0; i < o->Childrens.size(); i++) ReuniquifySubtree(o->Childrens[i]);
}

// ============================================================================
//  LA ANIM ACTIVA CAMBIO -> SOLTAR LA SELECCION DE LAS FILAS DE VERTEX ANIM
//
//  La anim activa NO es un solo global: es el PAR (ActiveAnimKind, ActiveAnimMesh) mas el
//  currentAnim de esa malla. Las filas "objanim:/objvtx:/objuv:/objnrm:" del dope se resuelven
//  por la ACTIVA al usarlas, asi que TODO camino que la cambie tiene que soltar esa seleccion
//  (Timeline.h). Estaban cubiertos AnimSelVertex y DeleteVertexAnimOf (Properties.cpp), pero la
//  mitad de la MALLA se escapo: DeleteUndo la toca por los DOS lados -Detach() la pone en
//  NULL cuando lo borrado contiene la anim activa, y Apply() la RESTAURA al deshacer- y no
//  soltaba nada. Repro (test 'dopevtxundo'): anim de MallaZA activa, borrar A, elegir la de
//  MallaZB, Ctrl+Z -> ActiveAnimMesh vuelve a A con la seleccion de B puesta.
//  Se compara ANTES/DESPUES en vez de soltar siempre: soltar de gratis borraria la seleccion
//  del usuario en el caso comun (borrar un objeto que no tiene nada que ver con la anim activa).
static void ReleaseDopeIfChangeAnimVertex(int kindPrev, Mesh* meshPrev) {
    if (ActiveAnimKind != kindPrev || ActiveAnimMesh != meshPrev) DopeReleaseVertexAnim();
}

// ============================================================================
//  LIBERAR UN SUBARBOL DETACHADO: EL SUBARBOL ENTERO, NO SOLO LA RAIZ
//
//  El comando de borrado se lleva RAICES (DelEntry), y adentro de cada raiz viaja TODO su
//  subarbol: los descendientes que TAMBIEN estaban seleccionados (CollectDelete corta la
//  recursion en el primer nodo borrable, asi que un hijo seleccionado de un padre seleccionado
//  nunca es raiz propia), los elementos 2D de un UI (Detachar se saltea el reparentado a
//  proposito: la interfaz viaja completa) y TODO el contenido de una coleccion borrada con
//  'delete col'.
//
//  Hacer "delete raiz" y listo NO libera nada de eso, porque ~Object (Objects.cpp) NO borra a
//  sus hijos: LOS PROMUEVE al padre. Con la raiz detachada eso daba dos desastres, los dos
//  silenciosos y los dos disparados por el desalojo del historial (MAXU=100), o sea 100
//  acciones DESPUES de que el usuario borro:
//   (A) con Parent VIVO (el padre nunca se solto en Detachar) los nietos se PROMUEVEN a la
//       escena viva: el objeto borrado RESUCITA, sin ningun paso de undo que lo explique, y
//       encima mutilado (sus animaciones las descarta el destructor y sus vinculos ya se
//       pusieron en NULL al detachar). Si su nombre lo tomo otro mientras tanto, la escena
//       queda con DOS objetos homonimos y eso se GUARDA en el .w3d: a partir de ahi todo lo
//       que resuelve por nombre (FindObjectByName, object("...") de lua, targetName,
//       RailName, hueso<->vertex group) cae en el primero en pre-orden.
//   (B) con Parent == NULL (todo objeto de primer nivel nace asi: Object::Object solo mete el
//       puntero en SceneCollection->Childrens, no se pone de Parent) no hay a quien promover:
//       el subarbol queda inalcanzable y NO se libera nunca. Fuga.
//
//  Por eso libera con W3dFreeSubtree (Objects.h), que es el MISMO camino que usa el cierre
//  de proyecto: post-orden, sacandole la lista de hijos a cada dueno antes de destruirlo.
//  'hechos' es lo unico propio de aca: la red contra el doble free. Hoy las raices son
//  disjuntas por construccion (CollectDelete corta la recursion en el primer borrable y no
//  anida), pero eso es una propiedad del recolector de hoy y un doble delete no avisa.
// ============================================================================
static void FreeSubtreeDetached(Object* root, std::set<Object*>& done) {
    if (!root || done.count(root) > 0) return;
    std::vector<Object*> stack; stack.push_back(root);   // anotar el subarbol ANTES de liberarlo
    while (!stack.empty()) {
        Object* o = stack.back(); stack.pop_back();
        done.insert(o);
        for (size_t i = 0; i < o->Childrens.size(); i++) stack.push_back(o->Childrens[i]);
    }
    W3dFreeSubtree(root);
}

class DeleteUndo : public UndoCmd {
    std::vector<DelEntry> ents;
    std::vector<RepEntry> reps;    // hijos de los borrados, reparentados al abuelo
    std::vector<RefEntry> refs;    // refs de mallas (modificador/skinArmature) a los objetos borrados -> limpiar/restaurar
    bool repsReady;                // las reps (y refs) se computan UNA vez (en el 1er detach)
    std::vector<Object*>  selPrev; Object* actPrev; Camera* camPrev; Object* colPrev; // CollectionActive previa
    int animKindPrev; Armature* animArmPrev; Mesh* animMeshPrev; // seleccion de animacion previa (para restaurar al deshacer)
    bool inScene; // true = los objetos estan en la escena; false = los tiene este comando (detachados)
    bool freeing; // true mientras el destructor libera lo detachado (ver UnlinkDetached)
    // CREACION: los SERIALES de los descendientes que las raices creadas ya tenian al capturar (las
    // copias de los hijos de un Shift+D / Alt+D con padre e hijos elegidos, ver LinkDuplicate en
    // ObjectMode.cpp). Nacieron con la raiz y se van con ella aunque no esten elegidos: si no, el
    // Ctrl+Z los PRESERVABA como a los hijos de un borrado (quedaban colgados del abuelo). Por serial
    // y no por puntero: un objeto borrado despues no deja una direccion que se pueda reciclar.
    std::set<unsigned int> created;
    // La ANIMACION de los borrados se la lleva ESTE comando, igual que los objetos. Tiene que ser asi: las listas de
    // curvas referencian al objeto POR PUNTERO, y cuando el comando muere hace delete del objeto -> el puntero queda
    // colgando. El proximo objeto que se cree puede caer EN ESA MISMA DIRECCION y "heredar" la animacion del muerto
    // (paso: animar un cubo, borrarlo, crear otro cubo y encontrarlo ya animado). Al deshacer vuelven con su objeto.
    //
    // *** POR ESCENA, NO "la lista que este activa" (fallos [C] y [D] de la ronda 7) ***
    // Las curvas de objeto NO viven en una sola lista: hay UNA POR ANIMACION DE ESCENA, y las de la escena ACTIVA
    // estan en el global AnimationObjects (SetActiveScene / NewScene / DeleteActiveScene swapean la lista
    // ENTERA, Animation.cpp). Antes esto recorria y devolvia SOLO AnimationObjects, o sea "la escena que este
    // activa EN ESE MOMENTO", que es exactamente la regla R1 de Undo.h:
    //   [D] borrar un objeto animado con OTRA escena activa dejaba su AnimationObject en la escena vieja con un
    //       puntero al objeto que este mismo comando libera al caerse del stack (y el objeto NUEVO heredaba la
    //       animacion cuando el allocator reciclaba la direccion: el bug que este diseno existe para evitar);
    //   [C] deshacer despues de cambiar de escena devolvia la curva a la escena EQUIVOCADA (la de origen la perdia
    //       y la otra se quedaba con una curva que nunca tuvo).
    // Ahora cada curva se guarda CON SU ESCENA y vuelve AHI. 'pos' es solo el lugar que ocupaba DENTRO de esa
    // lista: se conserva porque es el orden que ve el dope sheet, y se clampea al insertar.
    //
    // *** LA ESCENA VA POR INDICE REMAPEADO, NO POR PUNTERO (ronda 8) ***
    // La ronda 7 la guardaba como SceneAnimation* CRUDO y lo "revalidaba" buscandolo en SceneAnimations. Esa
    // validacion es EXACTAMENTE la que Undo.h declara insuficiente para los elementos de una lista: borrar la
    // escena hace delete (Animation.cpp DeleteActiveScene) y crear otra hace new (NewScene), asi que el
    // allocator RECICLA el bloque y el puntero muerto vuelve a "validar" contra una escena NUEVA que nunca tuvo
    // esa curva. Reproducido: escenas [S0,S1] con la curva en S1, borrar el objeto, borrar S1, crear una escena
    // -misma direccion- y Ctrl+Z -> la curva caia en la escena nueva (test 'delescena').
    // Ahora es el INDICE en SceneAnimations, remapeado en RemapList igual que KeyframesUndo hace con KFList::idx.
    // Desde la fase 6 borrar una animacion de escena es DESHACIBLE (UndoDeleteActiveScene empuja el paso que la
    // devuelve a su posicion: familia (1)), asi que por LIFO la escena ya volvio cuando este paso aplica. El
    // remapeo queda para quien avise un corrimiento (UndoDeletedList): idx = -1 = su escena murio; ver
    // ReturnAnimations para que se hace con la curva huerfana.
    struct SavedAnim {
        int esc;               // (c) INDICE en SceneAnimations, remapeado por RemapList (-1 = escena borrada)
        std::string escName;   // (d) el nombre que tenia esa escena: solo para RESUCITARLA si murio
        int pos; AnimationObject a;
        SavedAnim() : esc(-1), pos(0) {}
    };
    std::vector<SavedAnim> anims;
    // la lista VIVA de curvas de la escena 'idx', o NULL si ese indice ya no existe. Las de la escena ACTIVA viven
    // en el global AnimationObjects; las de las demas, adentro de su SceneAnimation (son la misma lista, swapeada).
    static std::vector<AnimationObject>* SceneCurves(int idx) {
        InitSceneAnimations();
        if (idx < 0 || idx >= (int)SceneAnimations.size() || !SceneAnimations[idx]) return NULL;
        return (idx == ActiveSceneAnim) ? &AnimationObjects : &SceneAnimations[idx]->objects;
    }
    void RemoveChild(Object* p, Object* c) {
        for (size_t k = 0; k < p->Childrens.size(); k++)
            if (p->Childrens[k] == c) { p->Childrens.erase(p->Childrens.begin()+k); break; }
    }
    // Se lleva la animacion de CADA objeto que este comando borra (y de todo su subarbol, que se va con el) de
    // TODAS las escenas, no solo de la activa. Vuelve en Reatachar.
    void TakeAnimations() {
        anims.clear();
        // UNA sola pasada por lista (antes era una por objeto borrado, y cada erase
        // corria los indices de la siguiente -> las posiciones guardadas no habrian servido).
        std::set<Object*> mine;
        for (size_t i = 0; i < ents.size(); i++) {
            if (!ents[i].mine) continue;   // esa raiz sigue en la escena: su animacion no es de este comando
            std::vector<Object*> stack; stack.push_back(ents[i].obj);
            while (!stack.empty()) {
                Object* o = stack.back(); stack.pop_back();
                for (size_t k = 0; k < o->Childrens.size(); k++) stack.push_back(o->Childrens[k]);
                mine.insert(o);
            }
        }
        InitSceneAnimations();
        for (size_t s = 0; s < SceneAnimations.size(); s++) {
            SceneAnimation* esc = SceneAnimations[s]; if (!esc) continue;
            std::vector<AnimationObject>& lst = ((int)s == ActiveSceneAnim) ? AnimationObjects : esc->objects;
            int removed = 0;                              // cuantas saque YA de ESTA lista
            for (size_t a = 0; a < lst.size(); ) {
                if (mine.count(lst[a].obj) > 0) {
                    SavedAnim g; g.esc = (int)s; g.escName = esc->name; g.pos = (int)a + removed; g.a = lst[a];
                    anims.push_back(g);                   // 'pos' = la posicion ORIGINAL en SU lista
                    lst.erase(lst.begin() + a);
                    removed++;
                } else a++;
            }
        }
    }
    // *** CRITERIO PARA LA CURVA HUERFANA (le borraron la escena de origen) ***
    // Hasta la ronda 7 era un 'continue': la curva NO volvia NUNCA. Eso no es un no-op seguro, es PERDIDA
    // SILENCIOSA DE DATOS en un Ctrl+Z (el objeto vuelve pelado y la animacion no esta en ninguna escena,
    // sin un solo aviso). Las otras dos opciones que se evaluaron:
    //   - "devolverla a la escena ACTIVA": es meterle a una escena una curva que NUNCA tuvo, que es
    //     EXACTAMENTE el fallo [C] de la ronda 7 (y encima indistinguible del bug del puntero reciclado);
    //   - "mantenerla viva hasta que la escena vuelva": la escena no vuelve nunca, borrarla no es
    //     deshacible -> es la perdida silenciosa con otro nombre.
    // Por eso se RESUCITA la escena: se crea una SceneAnimation nueva con el nombre que tenia (o el libre
    // mas parecido, la regla comun de nombres) y la curva entra AHI. Los datos no se pierden, ninguna
    // escena viva recibe nada ajeno, y el usuario ve por que aparecio (Notificar). Se crea UNA por escena
    // muerta: las entradas se agrupan por el nombre guardado, que era unico al capturar. La escena nace
    // al FINAL de la lista, asi no corre ningun indice de escena guardado en otro paso del undo.
    int ReviveScene(const std::string& name, std::vector<std::pair<std::string,int> >& done) {
        for (size_t i = 0; i < done.size(); i++) if (done[i].first == name) return done[i].second;
        InitSceneAnimations();
        const std::string free = SceneAnimFreeName(name.empty() ? "Scene" : name, -1);
        SceneAnimations.push_back(new SceneAnimation(free));
        const int idx = (int)SceneAnimations.size() - 1;
        done.push_back(std::make_pair(name, idx));
        w3dLogfW("[undo] curva huerfana: la escena '%s' ya no existe -> recreada como '%s'",
                 name.c_str(), free.c_str());
        W3dAvisof(false, T("Animation recovered in a new scene: '%s'"), W3dShortName(free).c_str());
        return idx;
    }
    void ReturnAnimations() {
        // van en orden creciente de pos DENTRO de cada escena (se recolectaron escena por escena),
        // asi cada una entra en su hueco: la anterior ya lo dejo listo.
        std::vector<std::pair<std::string,int> > revived;   // nombre de la escena muerta -> indice recreado
        for (size_t i = 0; i < anims.size(); i++) {
            if (anims[i].esc < 0) anims[i].esc = ReviveScene(anims[i].escName, revived);
            std::vector<AnimationObject>* lst = SceneCurves(anims[i].esc);
            if (!lst) continue;                            // no deberia pasar (el indice se acaba de resolver)
            int p = anims[i].pos;
            if (p < 0) p = 0;
            if (p > (int)lst->size()) p = (int)lst->size(); // la lista pudo achicarse
            lst->insert(lst->begin() + p, anims[i].a);
        }
        anims.clear();
    }

    // ========================================================================
    //  EL HIJO PRESERVADO PUDO MUDARSE ENTRE UN Detachar Y EL SIGUIENTE
    //
    //  Los 'reps' (los hijos NO seleccionados, que se preservan subiendolos al abuelo) se
    //  computaban UNA sola vez, en el constructor, y despues el undo/redo los replayaba
    //  ASUMIENDO que el hijo seguia donde el comando lo dejo. Falso: REPARENTAR NO ES
    //  DESHACIBLE (ObjectMode.cpp, W3dAttachTo) o sea que no vacia el stack de redo, asi
    //  que el usuario puede mover el hijo a OTRO padre en el medio y el camino queda abierto
    //  por los dos lados. Como RemoveChild(p,c) solo saca 'c' de p->Childrens, sacarlo del
    //  padre EQUIVOCADO era un no-op y el push_back metia el MISMO Object* en una SEGUNDA
    //  lista de hijos: el outliner lo listaba dos veces, el .w3d lo guardaba duplicado y
    //  -cuando el comando se caia del stack- W3dFreeSubtree liberaba al hijo aliaseado
    //  mientras la escena VIVA lo seguia apuntando (SEGV en la primera pasada del arbol).
    //
    //  Ahora son DOS reglas, y las dos hacen falta:
    //   (1) los reps se RECOMPUTAN en cada Detachar (o sea tambien en el REDO): lo que el
    //       usuario parento bajo el borrado despues del Ctrl+Z tambien se preserva, en vez
    //       de irse detachado con el comando (perdida silenciosa).
    //   (2) cada movimiento se hace SOLO si el hijo sigue donde este comando lo espera, y
    //       solo se DESHACE si fue este comando el que lo movio ('movida'). Si el usuario lo
    //       mudo a proposito, reponerlo no es lo que pidio... y sobre todo no se aliasea.
    // ========================================================================
    // ========================================================================
    //  PASO 0: DONDE ESTA HOY CADA RAIZ (cinturon y tiradores del FALLO A)
    //
    //  El redo buscaba la raiz borrada SOLO en e.parent->Childrens. Si el usuario la habia
    //  MUDADO mientras el comando esperaba en el stack de redo, no la encontraba, no la
    //  sacaba de ningun lado... y aun asi ponia inScene=false: el comando pasaba a creerse
    //  dueno de un objeto que seguia VIVO y colgado en la escena (aliasing -> .w3d con el
    //  nombre repetido -> SEGV al desalojarse el historial). Desde esta ronda reparentar SI
    //  es deshacible y vacia el redo, o sea que la ventana esta cerrada por arriba; esto es
    //  el piso, para cualquier camino que mueva un objeto sin dejar paso de undo.
    //  DOS reglas: (1) si no esta donde el comando lo dejo, se resuelve el padre REAL
    //  (Object::Parent) y se ACTUALIZA e.parent/e.index; (2) si no esta en NINGUNA lista de
    //  hijos, la entrada NO se toca y NO se declara propia (e.mine queda en false): nunca
    //  "dueno" de algo que no se saco de verdad.
    // ========================================================================
    static int IndexOf(Object* cont, Object* o) {
        if (!cont) return -1;
        for (size_t k = 0; k < cont->Childrens.size(); k++) if (cont->Childrens[k] == o) return (int)k;
        return -1;
    }
    void ResolveParents() {
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
            e.mine = false;
            if (!e.obj) continue;
            int k = IndexOf(e.parent, e.obj);
            if (k < 0) {
                Object* real = e.obj->Parent ? e.obj->Parent : SceneCollection;
                k = IndexOf(real, e.obj);
                if (k >= 0) {
                    w3dLogfW("[undo] '%s' se mudo de padre mientras el borrado estaba en el historial: "
                             "se lo saca de su padre REAL", e.obj->name.c_str());
                    e.parent = real;
                }
            }
            if (k < 0) {   // no esta en ninguna lista de hijos: no es de este comando
                w3dLogfE("[undo] '%s' no cuelga de ningun padre: el redo del borrado lo saltea "
                         "(no se lo declara detachado)", e.obj->name.c_str());
                continue;
            }
            e.index = k;
            e.mine = true;   // el paso 3 hace el erase de verdad
        }
    }
    void ComputeReps() {
        reps.clear();
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
                if (!e.mine) continue;   // esa raiz no se va a detachar: sus hijos no se mueven
                // borrar un UI se lleva TODA su interfaz: sus elementos 2D son PARTE del UI
                // (no tiene sentido dejarlos huerfanos en la escena). El subarbol entero viaja
                // detachado con el comando y vuelve completo al deshacer.
                if (e.obj->getType() == ObjectType::ui) continue;
                // idem una INSTANCIA DE PREFAB o un PROXY de una libreria: lo que cuelga de ellos lo GENERARON (no es
                // del usuario, no se guarda). Preservarlo lo dejaba suelto en el nivel como objetos normales -editables,
                // sin candado, con sus scripts corriendo y guardados con recursos de la libreria-: se va con ellos
                if (W3dIsInstanceType(e.obj->getType())) continue;
                // BASE (ver Objects.h): 'posUnderGrandparent' se ESCRIBE en ch->pos y va al .w3d. Con la
                // efectiva, borrar un padre con billboard le horneaba al hijo la orientacion de la
                // ultima camara adentro de su posicion, en silencio y sin vuelta atras.
                Vector3 gGrandparent = e.parent->GetGlobalPositionBase();
                // recorre el SUBARBOL del borrado: los descendientes NO seleccionados se PRESERVAN (reparent al abuelo);
                // los SELECCIONADOS se borran con el (se baja a sus hijos). Antes reparentaba TODO hijo directo -> los
                // hijos seleccionados (ej. las mallas de un armature al "borrar todo") sobrevivian en vez de borrarse.
                std::vector<Object*> stack; for (size_t k = 0; k < e.obj->Childrens.size(); k++) stack.push_back(e.obj->Childrens[k]);
                for (size_t k = 0; k < stack.size(); k++) {
                    Object* ch = stack[k];
                    bool isRoot = false;
                    for (size_t j = 0; j < ents.size(); j++) if (ents[j].obj == ch) { isRoot = true; break; }
                    if (isRoot) continue; // ya es su propio delete-root (no deberia caer dentro de otro subarbol)
                    // seleccionado (o CREADO con la raiz: ver 'creados') -> se borra con el subarbol; seguir bajando
                    if (ch->select || created.count(ch->serial)) {
                        // (una instancia o un proxy borrado se lleva lo que genero: no se baja a preservarlo)
                        if (!W3dIsInstanceType(ch->getType()))
                            for (size_t j = 0; j < ch->Childrens.size(); j++) stack.push_back(ch->Childrens[j]);
                        continue;
                    }
                    // NO seleccionado -> preservar: reparent al abuelo (no se recursea adentro; queda como estaba)
                    RepEntry r; r.child = ch; r.grandparent = e.parent; r.deleted = ch->Parent ? ch->Parent : e.obj;
                    r.posUnderDeleted     = ch->pos;
                    r.posUnderGrandparent = ch->GetGlobalPositionBase() - gGrandparent; // preserva posicion global (BASE)
                    reps.push_back(r);
                }
        }
    }

    void Detach() {
        // 0) DONDE ESTA HOY CADA RAIZ (ver ResolveParents): puede haberse mudado desde que este
        //    comando la dejo. Va ANTES de todo lo demas porque decide QUE raices son de este
        //    comando, y de eso dependen las replicas de los hijos y las animaciones.
        ResolveParents();
        // 1) el reparentado de los hijos de cada objeto borrado hacia el ABUELO. Un hijo que
        //    TAMBIEN se borra (esta en ents) se va con su root -> no se reparenta. Se RECOMPUTA
        //    en cada pasada (tambien en el redo): ver el bloque de arriba.
        ComputeReps();
        if (!repsReady) {
            // refs por PUNTERO al subarbol borrado: de mallas (modificador Armature/Mirror/Boolean o
            // skinArmature) y de objetos (familia Target + riel de camara, ver RefEntry arriba). Estas
            // SI se recolectan una sola vez: el que referencia ya solto puntero Y nombre y no hay accion
            // deshacible que los pueda volver a escribir sin vaciar el stack de redo.
            if (SceneCollection) CollectRefs(SceneCollection, ents, refs);
            repsReady = true;
        }
        // 2) mover los hijos reparentados al abuelo (ANTES de detachar el borrado, asi sus luces no se detachan).
        //    Solo si el hijo SIGUE bajo el borrado: si el usuario lo mudo (reparentar no es deshacible), no se
        //    toca -y no se aliasea el puntero en dos listas de hijos-.
        for (size_t i = 0; i < reps.size(); i++) { RepEntry& r = reps[i];
            r.moved = false;
            if (!r.child || !r.grandparent || !r.deleted) continue;
            if (r.child->Parent != r.deleted) continue;
            RemoveChild(r.deleted, r.child);
            r.child->Parent = r.grandparent;
            r.child->pos = r.posUnderGrandparent;
            r.grandparent->Childrens.push_back(r.child);
            r.moved = true;
        }
        // 3) detachar cada objeto borrado (ya sin esos hijos)
        const int kindPrev = ActiveAnimKind; Mesh* const meshPrev = ActiveAnimMesh; // ver ReleaseDopeIfChangeAnimVertex
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
            // el paso 0 ya resolvio padre e indice; si la raiz no aparecio en NINGUNA lista de
            // hijos, e.mine es false y la entrada se saltea ENTERA (tampoco se apagan sus luces:
            // el objeto sigue a la vista y quedaria oscuro sin motivo)
            if (!e.mine) continue;
            // el INDICE se re-resuelve POR PUNTERO justo antes del erase (es la propiedad que
            // la tabla de Undo.h le adjudica a DelEntry::index): el paso 2 y los erase de las
            // otras raices del mismo padre pueden haber corrido las posiciones.
            const int k = IndexOf(e.parent, e.obj);
            if (k < 0) { e.mine = false; continue; }
            e.index = k;
            e.parent->Childrens.erase(e.parent->Childrens.begin() + k);
            DetachLights(e.obj);
            if (ContainsCamActive(e.obj)) CameraActive = NULL;
            if (ContainsAnimActive(e.obj)) { ActiveAnimArm = NULL; ActiveAnimMesh = NULL; ActiveAnimKind = 0; } // anim activa (clip o vertex) borrada -> a ESCENA
        }
        // 4) limpiar las refs colgantes: el modificador vuelve a target=NULL ("none") y skinArmature a NULL -> la malla
        //    deja de deformar (vuelve a bind) y no queda apuntando al esqueleto borrado (que este comando liberara).
        //    Lo mismo la familia Target (instancia/espejo/camara/gamepad), la fuente de cada constraint
        //    y el riel: sueltan el puntero
        //    -asi la instancia deja de DIBUJAR al borrado- y tambien el nombre, para que el .w3d no quede guardado
        //    apuntando a un objeto que ya no esta en el archivo.
        for (size_t i = 0; i < refs.size(); i++) RefWrite(refs[i], false);
        // 5) si CollectionActive quedo FUERA de la escena (se borro), reapuntarla a una coleccion valida (otra bajo la
        //    escena, o la propia SceneCollection). Sin esto, Add Collection / Import parentan a memoria liberada
        //    (no aparecen / crash). El undo la restaura (colPrev).
        if (SceneCollection && (!CollectionActive || !InSubtree(SceneCollection, CollectionActive))) {
            Object* c = NULL;
            for (size_t i = 0; i < SceneCollection->Childrens.size() && !c; i++)
                if (SceneCollection->Childrens[i]->getType() == ObjectType::collection) c = SceneCollection->Childrens[i];
            CollectionActive = c ? c : SceneCollection;
        }
        TakeAnimations();   // la animacion se va CON el objeto: si no, su entrada queda apuntando a un muerto
        PurgeSelection();   // ni ActiveObj ni ObjSelects pueden quedar apuntando a lo detachado
        ReleaseDopeIfChangeAnimVertex(kindPrev, meshPrev); // la anim activa se fue con lo borrado
        inScene = false;
    }
    // esta 'o' dentro de alguno de los subarboles que este comando se lleva?
    bool InDeleted(Object* o) const {
        if (!o) return false;
        for (size_t i = 0; i < ents.size(); i++)
            if (ents[i].mine && InSubtree(ents[i].obj, o)) return true;   // solo lo que se llevo de verdad
        return false;
    }
    // El BORRADO REAL (Eliminar, ObjectMode.cpp) deja ActiveObj=NULL y ObjSelects vacio. El
    // Detach() del comando NO lo hacia, asi que el REDO (Ctrl+Y) dejaba los globales apuntando
    // a objetos DETACHADOS de los que este comando es DUENO: cuando el comando se cae del tope
    // del stack (MAXU) su destructor los libera y ActiveObj/ObjSelects quedan COLGADOS -> SEGV en
    // la proxima accion que los use ("add cube / selobj Cubo / delete / undo / redo" + 110 pasos).
    // Se sacan SOLO los que se van con el borrado (no se vacia la seleccion entera): asi el redo
    // de un JOIN -que es geo + DeleteUndo de los mergeados- conserva activa la malla que queda.
    void PurgeSelection() {
        for (size_t i = 0; i < ObjSelects.size(); ) {
            if (InDeleted(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + i);
            else i++;
        }
        if (InDeleted(ActiveObj)) ActiveObj = NULL;
    }
public:
    DeleteUndo(bool incCol) : repsReady(false), actPrev(NULL), camPrev(NULL), colPrev(NULL), inScene(true), freeing(false) {
        // la vista de un clip de jerarquia que se esta editando va a su clip ANTES de que su raiz (o un nodo suyo)
        // salga del arbol: detachada, la vista deja de escribirse
        W3dClipsViewsSync();
        selPrev = ObjSelects; actPrev = ActiveObj; camPrev = CameraActive; colPrev = CollectionActive;
        animKindPrev = ActiveAnimKind; animArmPrev = ActiveAnimArm; animMeshPrev = ActiveAnimMesh;
        if (SceneCollection) CollectDelete(SceneCollection, incCol, ents);
        Detach(); // el borrado YA paso: los saca de la escena (sin liberar)
    }
    // CREACION (Shift+D / Alt+D): los objetos seleccionados YA estan en la escena y NO se
    // detachan ahora. El comando nace "en escena": su primer Aplicar (el Ctrl+Z) los detacha como
    // un borrado y el Ctrl+Y los devuelve. Las reps/refs se computan en ese primer Detachar.
    struct Creation {};
    DeleteUndo(bool incCol, Creation) : repsReady(false), actPrev(NULL), camPrev(NULL), colPrev(NULL), inScene(true), freeing(false) {
        selPrev = ObjSelects; actPrev = ActiveObj; camPrev = CameraActive; colPrev = CollectionActive;
        animKindPrev = ActiveAnimKind; animArmPrev = ActiveAnimArm; animMeshPrev = ActiveAnimMesh;
        if (SceneCollection) CollectDelete(SceneCollection, incCol, ents);
        // lo que cuelga de cada raiz recien creada tambien es nuevo (ver 'creados')
        for (size_t i = 0; i < ents.size(); i++) {
            std::vector<Object*> stack(ents[i].obj->Childrens.begin(), ents[i].obj->Childrens.end());
            while (!stack.empty()) {
                Object* o = stack.back(); stack.pop_back();
                if (!o) continue;
                created.insert(o->serial);
                for (size_t k = 0; k < o->Childrens.size(); k++) stack.push_back(o->Childrens[k]);
            }
        }
    }
    bool IsEmpty() const { return ents.empty(); }
    void Apply() {
        if (inScene) { Detach(); return; } // redo del borrado
        // undo: re-inserta cada root en su padre (under->height: los indices guardados quedan validos).
        // SOLO las que este comando saco de verdad (e.mine): una raiz que no se pudo detachar sigue
        // colgada en la escena y re-insertarla la dejaria en DOS listas de hijos.
        for (int i = (int)ents.size()-1; i >= 0; i--) { DelEntry& e = ents[i];
            if (!e.mine) continue;
            int idx = e.index; if (idx < 0) idx = 0; if (idx > (int)e.parent->Childrens.size()) idx = (int)e.parent->Childrens.size();
            e.parent->Childrens.insert(e.parent->Childrens.begin()+idx, e.obj);
            ReattachLights(e.obj);
            e.mine = false;   // vuelve a ser de la escena
        }
        // devolver los hijos reparentados a su objeto original (restaurando su posicion local).
        // SOLO los que movio ESTE comando y que siguen donde los dejo: si el usuario los mudo
        // despues (reparentar no es deshacible, no vacia el redo), reponerlos no es lo que pidio
        // y ademas dejaria el MISMO Object* en dos listas de hijos -> doble free / SEGV.
        for (size_t i = 0; i < reps.size(); i++) { RepEntry& r = reps[i];
            if (!r.moved) continue;
            r.moved = false;
            if (!r.child || !r.grandparent || !r.deleted) continue;
            if (r.child->Parent != r.grandparent) continue;   // el usuario lo mudo: se queda donde esta
            RemoveChild(r.grandparent, r.child);
            r.child->Parent = r.deleted;
            r.child->pos = r.posUnderDeleted;
            r.deleted->Childrens.push_back(r.child);
        }
        // restaurar las refs (el armature/target volvio a la escena): el modificador recupera su target y skinArmature
        // su puntero -> la malla vuelve a deformar como antes del borrado. La familia Target recupera puntero Y nombre.
        for (size_t i = 0; i < refs.size(); i++) RefWrite(refs[i], true);
        // apagar la seleccion ACTUAL antes de restaurar la guardada (como SelectUndo):
        // sino un objeto seleccionado despues del borrado quedaba con select=true
        // fuera de ObjSelects (y el proximo Delete se lo llevaba puesto)
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = false;
        CameraActive = camPrev; ObjSelects = selPrev; ActiveObj = actPrev; CollectionActive = colPrev; // restaura seleccion + camara + coleccion activa
        {   // restaura la seleccion de animacion (el esqueleto o la vertex anim volvio). Es un
            // CAMINO MAS que cambia la anim activa: la seleccion del dope de las filas de vertex
            // anim se resuelve por la ACTIVA, asi que si la malla activa cambia hay que soltarla
            // (ver ReleaseDopeIfChangeAnimVertex arriba).
            const int kindPrev = ActiveAnimKind; Mesh* const meshPrev = ActiveAnimMesh;
            ActiveAnimKind = animKindPrev; ActiveAnimArm = animArmPrev; ActiveAnimMesh = animMeshPrev;
            ReleaseDopeIfChangeAnimVertex(kindPrev, meshPrev);
        }
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = true;
        ReturnAnimations();   // vuelve el objeto -> vuelve su animacion
        // NOMBRES (fallo F2): mientras los objetos estaban detachados, su nombre pudo
        // quedar TOMADO ("add cube / delete / add cube / undo" dejaba dos 'Cubo.001').
        // Se re-uniquifica el subarbol que vuelve, con la puerta de siempre (SetNameObj
        // se excluye a si mismo: si el nombre sigue libre no cambia nada). NO se arrastra
        // ningun vinculo: el que conserva el nombre es el que ya estaba en la escena, o
        // sea al que TODO lo que resuelve por nombre ya venia apuntando.
        for (size_t i = 0; i < ents.size(); i++) ReuniquifySubtree(ents[i].obj);
        inScene = true;
    }
    // detachados: liberar de verdad. 'liberando' hace que el recorrido de abajo NO entre a este
    // comando mientras dura: sus objetos se estan destruyendo y no hay que tocarlos desde afuera
    // (ademas es un lazo: el comando todavia esta en el stack cuando corre su destructor, porque
    // Push hace 'delete g_undo.front()' ANTES del erase).
    // OJO: el SUBARBOL entero, no solo las raices (ver FreeSubtreeDetached arriba). Los
    // hijos que se PRESERVARON (reps) no estan adentro: Detachar ya los movio al abuelo, en la
    // escena viva, antes de que este comando se quedara con nada.
    ~DeleteUndo() {
        freeing = true;
        if (!inScene) {
            std::set<Object*> done;
            // SOLO lo que este comando saco de verdad del arbol (e.mine): liberar una raiz que
            // quedo colgada en la escena es el use-after-free del fallo A.
            for (size_t i = 0; i < ents.size(); i++)
                if (ents[i].mine) FreeSubtreeDetached(ents[i].obj, done);
        }
    }
    // 'borrado' se libera: los subarboles que este comando tiene DETACHADOS son inalcanzables
    // desde SceneCollection, asi que el recorrido del destructor de Object no les llega y se
    // quedaban con el puntero muerto (y con la direccion reciclada, apuntando a OTRO objeto).
    void UnlinkDetached(Object* deleted) {
        // el DUENO de una ref es un objeto que estaba VIVO al capturar (no es de este comando):
        // si lo liberan, RefWrite escribiria sobre memoria muerta -o sobre el objeto que
        // reciclo la direccion-. La entrada se mata (dueno=NULL -> RefWrite es no-op).
        for (size_t i = 0; i < refs.size(); i++)
            if (refs[i].owner == deleted) { refs[i].owner = NULL; refs[i].mod = NULL; }
        // idem para los hijos PRESERVADOS: el hijo y el abuelo NO son de este comando (viven en la
        // escena) y el borrado si lo es. Si cualquiera de los tres se libera, la replica queda MUERTA
        // (child=NULL -> los dos replays la saltean) en vez de deshacerse sobre memoria liberada.
        for (size_t i = 0; i < reps.size(); i++)
            if (reps[i].child == deleted || reps[i].grandparent == deleted || reps[i].deleted == deleted) {
                reps[i].child = NULL; reps[i].grandparent = NULL; reps[i].deleted = NULL; reps[i].moved = false;
            }
        if (freeing || inScene) return;   // inScene: sus objetos ya los camina el arbol vivo
        for (size_t i = 0; i < ents.size(); i++) {
            if (!ents[i].mine) continue;      // esa raiz sigue en la escena: la camina el arbol vivo
            std::vector<Object*> stack; stack.push_back(ents[i].obj);
            while (!stack.empty()) {
                Object* o = stack.back(); stack.pop_back();
                if (!o || o == deleted) continue;
                o->UnlinkFrom(deleted);
                for (size_t k = 0; k < o->Childrens.size(); k++) stack.push_back(o->Childrens[k]);
            }
        }
    }
    // ents/reps/selPrev: (a) punteros a Object/Mesh, propiedad de ESTE comando mientras estan
    // detachados (y los que NO son suyos se sueltan en UnlinkDetached, arriba).
    // refs: (a) pero OJO, no todos con el mismo argumento -y decir "todos revalidados" era FALSO
    // hasta la ronda 11-:
    //   - refs[].target es de ESTE comando (esta detachado adentro de ents) -> vive lo que vive
    //     el comando;
    //   - refs[].owner NO es suyo: es un objeto que estaba vivo al capturar. Se suelta cuando lo
    //     destruyen (UnlinkDetached de arriba) y con dueno=NULL RefWrite es no-op;
    //   - refs[].mod TAMPOCO es suyo y ni siquiera es un Object: es un Modifier* del stack de una
    //     malla ajena, que ~Object NO enumera (no es una ref de objeto) y que se libera por su
    //     cuenta. Por eso se revalida al escribir, por PUNTERO + SERIAL, contra el stack de su
    //     malla (LiveModIn / RefWrite). Sin eso el undo escribia el target encima del
    //     modificador de OTRA malla y lo guardaba en el .w3d: test 'modtargetuaf'.
    // refs[].modSerial es (d) -una copia del numero de identidad-.
    // anims[].esc es (c): el INDICE de la escena en SceneAnimations, que es una lista de familia (2) -el "-"
    // de la tarjeta Animation borra una escena SIN empujar ningun paso de undo- y por eso se remapea abajo.
    // ANTES estaba clasificado como (a) "puntero revalidado contra SceneAnimations", y era falso: esa
    // revalidacion es la que Undo.h descarta para elementos de una lista (delete + new = direccion reciclada).
    // anims[].escName es (d) y anims[].a es (b) -una copia por valor de la AnimationObject-.
    // anims[].pos es (c) contra la lista de curvas de ESA escena: NO es una lista con nombre (no tiene
    // destinos de rename ni pasa por RemapInStacks) y desde la ronda 6 nadie la indexa (KeyframesUndo va por
    // identidad de curva), asi que 'pos' es solo el ORDEN que ve el dope sheet y se clampea al insertar.
    // DelEntry::index es (c) -una posicion en Parent->Childrens- pero esa lista NO es una lista
    // con nombre: no tiene destinos de rename por indice ni pasa nunca por RemapInStacks, y
    // ademas se RE-RESUELVE por puntero en cada Detach() y se clampea al re-insertar. Si algun
    // dia el orden de los hijos se volviera indexable, este comando tiene que remapearla.
    void RemapList(const W3dRenameDest& list, int a, int b) {
        if (!W3dSameList(W3dDestGlobal(W3dRenameDest::SceneAnimG, -1), list)) return;
        for (size_t i = 0; i < anims.size(); i++) {
            if (anims[i].esc < 0) continue;                       // ya huerfana
            if (!RemapIndexSaved(anims[i].esc, a, b)) anims[i].esc = -1;  // borraron SU escena
        }
    }
};

// ============================================================================
//  REPARENT DESHACIBLE (el Ctrl+Z de un Ctrl+P) - ronda 14
//
//  POR QUE ESTO ES UN ARREGLO DE MEMORIA Y NO UNA COMODIDAD
//  Un comando que espera en el stack de REDO guarda POSICIONES DEL ARBOL y las replaya al
//  rehacer: DeleteUndo guarda (padre, indice) de cada raiz borrada y el padre de cada hijo
//  preservado. Mientras reparentar NO empujaba ningun paso, mover un objeto NO vaciaba el
//  redo, asi que el usuario podia dejar el arbol distinto del que el comando espera:
//    "add cube ; selobj Cubo ; delete ; undo ; parent Cubo Cubo.001 ; redo"
//  El redo del borrado no encontraba la raiz en e.parent->Childrens, no la sacaba de ningun
//  lado... y aun asi se declaraba DUENO de ella -> el mismo Object* vivo en la escena Y en el
//  comando: el outliner lo listaba dos veces, el .w3d salia con el nombre repetido y el
//  desalojo del historial lo liberaba con la escena apuntandolo (SEGV). La ronda 13 tapo el
//  caso del HIJO preservado con un guard por entrada; la RAIZ seguia abierta, y con ella
//  cualquier otro comando futuro que guarde posiciones del arbol.
//  Con el reparent deshacible el habilitador desaparece: empujar el paso VACIA el stack de
//  redo (Push -> ClearRedo) y no queda ningun comando "de ida" esperando un arbol viejo.
//  (El guard de Detachar quedo igual, como cinturon y tiradores: ver el paso 3.)
//
//  QUE GUARDA Y POR QUE
//   - el padre CRUDO (Object::Parent), que NO es lo mismo que SceneCollection: los objetos de
//     primer nivel nacen con Parent == NULL y viven igual en SceneCollection->Childrens
//     (Object::Object). Guardar "SceneCollection" ahi cambiaria el estado al deshacer.
//   - la POSICION entre los hermanos: el drag&drop del outliner REORDENA sin cambiar de padre,
//     y ese orden es el que ve el usuario (y el que se guarda en el .w3d).
//   - el TRANSFORM LOCAL: los caminos "Keep Transform" reescriben pos/rot/escala DESPUES de la
//     cirugia de punteros, asi que sin esto el Ctrl+Z devolvia el objeto a su padre con las
//     coordenadas del padre NUEVO (saltaba de lugar).
//  Apply() es el swap de siempre: el mismo codigo deshace y rehace.
//
//  NO-OP SEGURO (las dos formas de morir de esta familia, ver Undo.h)
//   - si el objeto o el padre guardado se LIBERAN, UnlinkDetached mata la entrada. Ese
//     virtual es justo el que se omitio en la ronda 12 y produjo el bloqueante de aquella vez;
//   - si el objeto o el padre estan DETACHADOS (los tiene un DeleteUndo, fuera del arbol) no se
//     toca NADA: re-insertarlos meteria el mismo puntero en dos listas de hijos, que es
//     exactamente el bug que este comando viene a cerrar. Por eso se chequea que los dos sean
//     ALCANZABLES desde SceneCollection antes de mover nada.
// ============================================================================
class ReparentUndo : public UndoCmd {
    Object*    obj;
    Object*    parent;    // CRUDO: NULL = primer nivel (vive en SceneCollection->Childrens)
    int        idx;       // posicion entre los hermanos
    Vector3    pos, rotEuler, scale;
    Quaternion rot;
    bool       live;     // false = lo liberaron: no-op seguro
    static Object* ContOf(Object* rawParent) { return rawParent ? rawParent : SceneCollection; }
    static int IndexIn(Object* cont, Object* o) {
        if (!cont) return -1;
        for (size_t i = 0; i < cont->Childrens.size(); i++) if (cont->Childrens[i] == o) return (int)i;
        return -1;
    }
    // alcanzable desde la raiz? (un objeto DETACHADO por un DeleteUndo no lo es)
    static bool InScene(Object* o) { return o && SceneCollection && InSubtree(SceneCollection, o); }
    void Read() {
        parent = obj->Parent; idx = IndexIn(ContOf(parent), obj);
        pos = obj->pos; rot = obj->Rot(); rotEuler = obj->rotEuler; scale = obj->scale;
    }
public:
    ReparentUndo(Object* o) : obj(o), parent(NULL), idx(-1), live(false) {
        if (!obj) return;
        live = true;
        Read();
    }
    bool IsEmpty() const { return !live || !obj; }
    // hubo mudanza real? Un drag que suelta al objeto donde ya estaba no merece un paso.
    bool Differs() const {
        if (IsEmpty()) return false;
        // se compara el CONTENEDOR, no el puntero crudo: los objetos de primer nivel nacen con
        // Parent == NULL y el embudo se lo normaliza a SceneCollection aunque no se muevan de
        // lugar. Son el mismo estado para todo el editor, y contarlo como cambio dejaba un paso
        // de undo que "no hace nada" en cada drag que suelta al objeto donde ya estaba.
        // (El undo igual RESTAURA el crudo, que es lo unico que devuelve el estado exacto.)
        if (ContOf(obj->Parent) != ContOf(parent)) return true;
        if (IndexIn(ContOf(obj->Parent), obj) != idx) return true;
        if (obj->pos.x != pos.x || obj->pos.y != pos.y || obj->pos.z != pos.z) return true;
        if (obj->rotEuler.x != rotEuler.x || obj->rotEuler.y != rotEuler.y || obj->rotEuler.z != rotEuler.z) return true;
        if (obj->scale.x != scale.x || obj->scale.y != scale.y || obj->scale.z != scale.z) return true;
        return false;
    }
    void Apply() {
        if (IsEmpty()) return;
        Object* destination = ContOf(parent);
        Object* current     = ContOf(obj->Parent);
        if (!destination || !current) return;
        if (!InScene(obj) || !InScene(destination)) return;   // detachado: NO tocar (ver el bloque de arriba)
        const int idxLive = IndexIn(current, obj);
        if (idxLive < 0) return;                            // no esta en la lista de su padre: no-op
        Object* parentLive = obj->Parent;
        Vector3 pl = obj->pos, el = obj->rotEuler, sl = obj->scale; Quaternion rl = obj->Rot();
        current->Childrens.erase(current->Childrens.begin() + idxLive);
        int k = idx; if (k < 0) k = 0;
        if (k > (int)destination->Childrens.size()) k = (int)destination->Childrens.size();
        destination->Childrens.insert(destination->Childrens.begin() + k, obj);
        obj->Parent = parent;
        obj->pos = pos; obj->scale = scale;
        obj->SetRotSnapshot(rot, rotEuler);   // tal cual se capturo (igual que TransformUndo)
        parent = parentLive; idx = idxLive; pos = pl; rot = rl; rotEuler = el; scale = sl;
    }
    // el objeto o el padre se estan LIBERANDO: la entrada queda muerta (no-op) en vez de
    // aplicarse sobre memoria liberada -o sobre el objeto que reciclo la direccion-.
    void UnlinkDetached(Object* deleted) {
        if (deleted && (deleted == obj || deleted == parent)) { live = false; obj = NULL; parent = NULL; }
    }
    // obj/padre: (a) punteros sueltos en UnlinkDetached. pos/rot/rotEuler/scale: (d).
    // idx: es una POSICION, pero en Parent->Childrens, que NO es una lista con nombre (no tiene
    // destinos de rename por indice y nunca pasa por RemapInStacks) - el MISMO argumento que
    // DelEntry::index de DeleteUndo -, y ademas se CLAMPEA al re-insertar. Si algun dia el orden
    // de los hijos se vuelve indexable, este comando necesita remapeo propio.
    W3D_UNDO_NO_INDICES
};

// ---- stacks ----
static std::vector<UndoCmd*> g_undo;
static std::vector<UndoCmd*> g_redo;
static TransformUndo*        g_pendingT  = NULL; // transform de objeto en curso (sin confirmar)
static EditMoveUndo*         g_pendingEM = NULL; // move de malla en edit mode en curso
static MaterialModUndo*      g_pendingMat = NULL; // modificacion de material en curso (checkbox/shininess)
static ReparentUndo*         g_pendingRep = NULL; // mudanza de padre en curso (ver ReparentUndo)
static const size_t          MAXU = 100;
// bases de los GRUPOS abiertos (UndoGroupStart/Fin): posiciones en g_undo. El desalojo del
// tope de abajo (MAXU) las corre, sino UndoGroupEnd fundiria pasos de ANTES del grupo.
static std::vector<size_t>   g_groups;

// SACAR DEL STACK ANTES DE HACER EL delete, siempre. Destruir un comando LIBERA los objetos que
// tiene detachados, y eso dispara ~Object -> el recorrido de abajo, que camina g_undo/g_redo: si
// el comando que se esta destruyendo (o uno ya destruido de la misma tanda) sigue en el vector,
// el recorrido llama a un metodo virtual sobre memoria liberada. Es el mismo bug que estamos
// cerrando, un nivel mas arriba.
static void DeleteCmds(std::vector<UndoCmd*>& v) {
    std::vector<UndoCmd*> tmp; tmp.swap(v);          // el vector queda VACIO antes del primer delete
    for (size_t i = 0; i < tmp.size(); i++) delete tmp[i];
}
static void ClearRedo() { DeleteCmds(g_redo); }

// desaloja el fondo del stack hasta respetar MAXU. Las bases de los grupos ABIERTOS son
// posiciones absolutas en g_undo: si no se corren con el desalojo, UndoGroupEnd funde de mas.
static void EvictOld() {
    while (g_undo.size() > MAXU) {
        UndoCmd* old = g_undo.front();
        g_undo.erase(g_undo.begin());   // fuera del stack ANTES del delete (ver DeleteCmds)
        delete old;
        for (size_t i = 0; i < g_groups.size(); i++) if (g_groups[i] > 0) g_groups[i]--;
    }
}

// LO NO GUARDADO (io/ProjectChanges.h): cada paso que se apila (y cada Ctrl+Z / Ctrl+Y) marca la raiz activa
// como editada. Las operaciones de la BIBLIOTECA (carpetas, renombrar un recurso) no son de la raiz: levantan
// g_undoNoRoot mientras apilan su paso.
extern void W3dTouchedChanges();
int g_undoNoRoot = 0;
static void Push(UndoCmd* c) {
    if (!c) return;
    ClearRedo(); // una accion NUEVA invalida el redo
    if (g_undoNoRoot <= 0) c->rootOf = SceneCollection;
    g_undo.push_back(c);
#ifdef W3D_MCP
    { extern void W3dHistoryUndo(const char*); W3dHistoryUndo(typeid(*c).name()); }   // la grabadora de pasos
#endif
    EvictOld();
    if (g_undoNoRoot <= 0) W3dTouchedChanges();
}
// el paso es de OTRA escena abierta: esa pasa a ser la activa antes de aplicarlo (io/EditorRoots.h). false = no se
// pudo (el paso se descarta)
static bool ActivateStepRoot(UndoCmd* c) {
    if (!c->rootOf || c->rootOf == SceneCollection) return true;
    extern bool W3dActivateRootOf(Object* root);
    return W3dActivateRootOf(c->rootOf);
}
// una escena que se cierra y se libera: sus pasos se van (nombran objetos que dejan de existir)
static void RemoveFromRoot(std::vector<UndoCmd*>& v, Object* root, std::vector<UndoCmd*>& outside) {
    for (size_t i = v.size(); i-- > 0; )
        if (v[i]->rootOf == root) { outside.push_back(v[i]); v.erase(v.begin() + (long)i); }
}
void UndoForgetRoot(Object* root) {
    if (!root) return;
    std::vector<UndoCmd*> outside;
    RemoveFromRoot(g_undo, root, outside);
    RemoveFromRoot(g_redo, root, outside);
    if (!outside.empty()) g_groups.clear();   // (las bases de los grupos indexan el stack de antes)
    DeleteCmds(outside);
}

// ============================================================================
//  EL DESVINCULADO DEL DESTRUCTOR TIENE QUE ALCANZAR TAMBIEN A LO DETACHADO
//
//  ~Object suelta las referencias caminando el ARBOL VIVO (UnlinkFromTree, Objects.cpp).
//  Eso deja afuera a los objetos que los comandos del undo tienen DETACHADOS: no cuelgan de
//  SceneCollection, asi que nadie les avisa y se quedan con un puntero a memoria liberada. Con
//  la direccion reciclada por el allocator la referencia pasa a apuntar a OTRO objeto, en
//  silencio (es el modo de fallar de esta familia entera de bugs).
//
//  Esto NO reemplaza a CollectRefs: son dos cosas distintas y hacen falta las dos.
//   - CollectRefs/RefWrite es la parte del MODELO: al borrar, el que referencia suelta
//     puntero Y nombre (asi la instancia deja de dibujar al borrado y el .w3d no queda con un
//     target a un objeto ausente), y al deshacer los recupera. Eso el destructor no lo puede
//     hacer: cuando llega el free ya no hay nada que restaurar.
//   - esto de aca es la parte de MEMORIA, y es un piso: garantiza que NINGUN puntero sobreviva
//     al free venga de donde venga. Hoy, con el desalojo FIFO del stack, el caso "referenciador
//     detachado que sobrevive a su target liberado" es dificil de armar; pero esa es una
//     propiedad del ORDEN DE DESALOJO de hoy, no del diseno, y no quiero que la seguridad de
//     memoria dependa de ella.
// ============================================================================
static void UndoUnlinkDetached(Object* deleted) {
    for (size_t i = 0; i < g_undo.size(); i++) g_undo[i]->UnlinkDetached(deleted);
    for (size_t i = 0; i < g_redo.size(); i++) g_redo[i]->UnlinkDetached(deleted);
}
// el gancho se pone solo al cargar el binario: no hay un "init del undo" al que sumarse, y
// olvidarse de llamarlo seria justo el agujero que esto tapa. La lista de ganchos arranca en
// NULL por inicializacion ESTATICA (constante), o sea antes que cualquier constructor global.
struct UndoHookUnlink {
    UndoHookUnlink() { W3dUnlinkRegister(UndoUnlinkDetached); }
};
static UndoHookUnlink g_undoHookUnlink;

// ============================================================================
//  LOS INDICES DE UNA LISTA CON NOMBRE SE CORREN: reordenar (botones "subir/bajar" de las
//  tarjetas) o BORRAR un elemento (boton "-"). Ver el bloque (A)/(B) de Undo.h.
//
//  Al reordenar faltaban DOS cosas:
//   (1) no dejaba NINGUN paso de undo: el orden se perdia y no habia Ctrl+Z;
//   (2) -peor- los destinos de rename ya capturados van por (dueno, lista, INDICE), y
//       reordenar los invalidaba EN SILENCIO: el Ctrl+Z de un rename anterior escribia
//       el nombre viejo ENCIMA del elemento de al lado (corrupcion muda, no un crash).
//  Por eso el intercambio, el paso de undo y el REMAPEO de los indices capturados salen
//  siempre del MISMO lugar: nadie puede reordenar "por su cuenta" y saltearse el remapeo.
//
//  LISTAS CUBIERTAS ACA (las que TIENEN botones de reordenar): vertex groups, UV groups,
//  UV maps y capas de color (todas de la malla) + los clips 3D del esqueleto, que viven en
//  Armature y por eso tienen su propio par swap/comando mas abajo. Si manana le aparecen
//  botones a otra (armatures 2D, vertex anims...), sumarla a los dos switch de aca abajo.
//  El comentario viejo decia que VGroup/UVGroup eran "las unicas listas con botones de
//  reordenar": era FALSO (uv maps, capas de color y clips 3D tambien tienen) y por eso esas
//  tres reordenaban a mano, sin undo y sin remapeo.
// ============================================================================
// LOS COMANDOS PENDIENTES TAMBIEN GUARDAN INDICES. Un comando "pendiente" (capturado con
// Iniciar y todavia sin Confirmar) NO esta en ningun stack: recorrer solo g_undo/g_redo lo
// deja afuera del remapeo y, cuando se confirma, entra al stack con los indices VIEJOS. Es el
// mismo bug de siempre pero un rato antes, y un RemapList impecable no lo tapa. Hoy no se
// conoce un camino de UI que corra una lista con un pendiente abierto (los Iniciar/Confirmar
// se cierran dentro de la misma operacion), pero eso es una propiedad de la UI de HOY, no del
// undo. Se define al FINAL del archivo: necesita ver TODOS los pendientes, y cada uno se
// declara al lado de su comando.
static void RemapInPending(const W3dRenameDest& list, int a, int b);
static void RemapInStacks(const W3dRenameDest& list, int a, int b) {
    for (size_t i = 0; i < g_undo.size(); i++) g_undo[i]->RemapList(list, a, b);
    for (size_t i = 0; i < g_redo.size(); i++) g_redo[i]->RemapList(list, a, b);
    RemapInPending(list, a, b);
}
static bool LayerSwapInMesh(Mesh* m, int layer, int a, int b) {
    if (!m || a == b) return false;
    if (layer == W3dRenameDest::VGroup) {
        if (a < 0 || b < 0 || a >= (int)m->vertexGroups.size() || b >= (int)m->vertexGroups.size()) return false;
        VertexGroup* t = m->vertexGroups[a]; m->vertexGroups[a] = m->vertexGroups[b]; m->vertexGroups[b] = t;
        return true;
    }
    if (layer == W3dRenameDest::UVGroup) {
        if (a < 0 || b < 0 || a >= (int)m->uvGroups.size() || b >= (int)m->uvGroups.size()) return false;
        UVGroup* t = m->uvGroups[a]; m->uvGroups[a] = m->uvGroups[b]; m->uvGroups[b] = t;
        return true;
    }
    if (layer == W3dRenameDest::UVMap) {
        if (a < 0 || b < 0 || a >= (int)m->uvMaps.size() || b >= (int)m->uvMaps.size()) return false;
        UVMap* t = m->uvMaps[a]; m->uvMaps[a] = m->uvMaps[b]; m->uvMaps[b] = t;
        return true;
    }
    if (layer == W3dRenameDest::ColorLayer) {
        if (a < 0 || b < 0 || a >= (int)m->colorLayers.size() || b >= (int)m->colorLayers.size()) return false;
        ColorLayer* t = m->colorLayers[a]; m->colorLayers[a] = m->colorLayers[b]; m->colorLayers[b] = t;
        return true;
    }
    return false;   // Arm2D / VertAnim: no tienen botones de reordenar
}
static int* ActiveLayerOf(Mesh* m, int layer) {
    if (!m) return NULL;
    if (layer == W3dRenameDest::VGroup)     return &m->activeGroup;
    if (layer == W3dRenameDest::UVGroup)    return &m->uvActiveGroup;
    if (layer == W3dRenameDest::UVMap)      return &m->uvActiveMap;
    if (layer == W3dRenameDest::ColorLayer) return &m->activeColor;
    return NULL;
}
// las capas POR CORNER (uv maps / capas de color) se hornean a los arrays de render segun cual
// es la ACTIVA: reordenar mueve el indice activo, asi que hay que re-hornear (los llamadores de
// los botones ya lo hacian; el UNDO tambien tiene que hacerlo o el viewport queda con la capa
// anterior). Las otras dos listas no se hornean: solo se invalida el cache de skinning.
static void LayerPostChange(Mesh* m, int layer) {
    if (!m) return;
    if (layer == W3dRenameDest::UVMap || layer == W3dRenameDest::ColorLayer) m->ApplyLayersToRender();
    m->lastSkinFrame = -999999; m->pose2dDirty = true;
    W3dMeshMarkEdited(m);   // el orden de las listas es dato de la malla: su RECURSO lo publica
}
class LayerOrderUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m; int layer, a, b, active;
public:
    LayerOrderUndo(Mesh* M, int C, int A, int B, int Act) : m(M), layer(C), a(A), b(B), active(Act) {}
    void Apply() {
        if (!m || !ObjectInScene(m)) return;               // la malla ya no esta: no-op SEGURO
        if (!LayerSwapInMesh(m, layer, a, b)) return;      // la lista cambio de tamano: no tocar
        int* act = ActiveLayerOf(m, layer);
        if (act) { int cur = *act; *act = active; active = cur; }  // swap (sirve de ida y de vuelta)
        RemapInStacks(W3dDestLayerMesh(m, layer, -1), a, b); // los renames capturados siguen a SU elemento
        LayerPostChange(m, layer);
    }
    // ESTE PASO TAMBIEN GUARDA INDICES. No alcanza con remapear los destinos de rename: un
    // paso de reordenar es "intercambiar las posiciones a y b", y si entre medio se BORRA un
    // elemento de la lista sin que ese borrado sea deshacible, las posiciones se corren y el
    // Ctrl+Z termina intercambiando el PAR EQUIVOCADO (no corrompe nombres, pero deja un
    // orden que nunca existio). Ver el criterio general en Undo.h.
    void RemapList(const W3dRenameDest& list, int x, int y) {
        if (!W3dSameList(W3dDestLayerMesh(m, layer, -1), list)) return;
        if (a < 0 || b < 0) return;                    // el paso ya estaba muerto
        // el activo guardado corre igual; si borraron JUSTO al que era el activo, ese indice ya
        // no significa nada (dejarlo apuntaria al VECINO, y el undo elegiria un activo raro).
        if (!RemapIndexSaved(active, x, y)) active = -1;
        if (!RemapIndexSaved(a, x, y) || !RemapIndexSaved(b, x, y))
            { a = -1; b = -1; }                        // borraron una de las dos puntas: el paso MUERE (no-op)
    }
};
bool UndoMoveLayerMesh(Mesh* m, int layer, int i, int j) {
    if (!m || i == j) return false;
    int* act = ActiveLayerOf(m, layer);
    if (!act) return false;
    const int actPrev = *act;
    if (!LayerSwapInMesh(m, layer, i, j)) return false;
    *act = j;                                 // el activo VIAJA con el elemento que se movio
    RemapInStacks(W3dDestLayerMesh(m, layer, -1), i, j);
    Push(new LayerOrderUndo(m, layer, i, j, actPrev));
    LayerPostChange(m, layer);
    return true;
}

// ---- CLIPS 3D (Armature::animations) ----------------------------------------------------
// DECISION: la lista vive en el CORE (SkeletalAnimation.cpp, que compila tambien para el
// runtime de los juegos, donde no hay undo ni stacks que remapear) y el remapeo es del EDITOR.
// En vez de meterle al Core una dependencia con el undo (o un hook con puntero a funcion), el
// swap se hace ACA -exactamente como LayerSwapInMesh hace el de las capas de la malla- y
// MoveActiveAnimation del Core queda SOLO para el runtime / la carga. El editor (tarjeta
// Animation y el harness de tests) llama UndoMoveClipArm. No hace falta ningun .cpp nuevo.
static bool ClipSwapInArm(Armature* a, int i, int j) {
    if (!a || i == j) return false;
    if (i < 0 || j < 0 || i >= (int)a->animations.size() || j >= (int)a->animations.size()) return false;
    SkeletalAnimation* t = a->animations[i]; a->animations[i] = a->animations[j]; a->animations[j] = t;
    // el cache de pose compara el INDICE del clip (lastPoseAnim): reordenar cambia los indices
    // sin cambiar el clip activo, asi que hay que invalidarlo o la pose se queda pegada.
    a->lastPoseFrame = -999999; a->lastPoseAnim = -999; a->poseDirty = true;
    return true;
}
// Las claves del dope llevan el INDICE DEL CLIP 3D adentro ("arm:#<serial rig>/k<CLIP>/b<n>", ver Timeline.h):
// reordenar o borrar clips corre esos indices igual que corre los del undo.
//
// *** EL AVISO VA ACOTADO AL RIG (ronda 8) ***
// La ronda 7 avisaba con el prefijo "arm:k" PELADO, sin el rig, porque el formato de la clave ponia el
// numero ANTES del dueno y DopeKeySplit exige el digito pegado al prefijo. Eso corria los indices de
// clip de las claves de TODOS los armatures, y el comentario que lo declaraba inofensivo ("el resolver
// chequea ademas el nombre del rig -> una clave ajena no resuelve ni antes ni despues") era FALSO: la
// clave ajena no resuelve MIENTRAS el rig activo sea otro, pero el usuario vuelve a SU rig y ahi la clave
// YA CORRIDA resuelve al CLIP EQUIVOCADO de su propio rig (test 'dopeclip3d'). El formato de la clave se
// dio vuelta a "arm:<rig>/k<CLIP>/..." (dueno primero, como el 2D) y el aviso ahora se acota:
static void DopeRemapClip3D(const Armature* a, int i, int j) {
    if (!a) return;
    DopeRemapIndexKey("arm:" + DopeIdOwner(a) + "/k", i, j);   // el dueno POR SERIAL (el nombre se recicla)
}
class ClipOrderUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)a == deleted) a = NULL; }
private:
    Armature* a; int i, j, active;
public:
    ClipOrderUndo(Armature* A, int I, int J, int Act) : a(A), i(I), j(J), active(Act) {}
    void Apply() {
        if (!a || !ObjectInScene(a)) return;      // el armature ya no esta: no-op SEGURO
        if (!ClipSwapInArm(a, i, j)) return;      // la lista cambio de tamano: no tocar
        int cur = a->activeAnim; a->activeAnim = active; active = cur;  // swap (ida y vuelta)
        RemapInStacks(W3dDestClipArm(a, -1), i, j);
        DopeRemapClip3D(a, i, j);                  // las claves del dope llevan el clip adentro (ver arriba)
    }
    // idem LayerOrderUndo::RemapList (este paso guarda indices, no identidades)
    void RemapList(const W3dRenameDest& list, int x, int y) {
        if (!W3dSameList(W3dDestClipArm(a, -1), list)) return;
        if (i < 0 || j < 0) return;                    // el paso ya estaba muerto
        if (!RemapIndexSaved(active, x, y)) active = -1; // borraron al que era el activo (ver LayerOrderUndo)
        if (!RemapIndexSaved(i, x, y) || !RemapIndexSaved(j, x, y))
            { i = -1; j = -1; }                        // borraron una de las dos puntas: el paso MUERE
    }
};
bool UndoMoveClipArm(Armature* a, int i, int j) {
    if (!a || i == j) return false;
    const int actPrev = a->activeAnim;
    if (!ClipSwapInArm(a, i, j)) return false;
    a->activeAnim = j;                        // el activo VIAJA con el clip que se movio
    RemapInStacks(W3dDestClipArm(a, -1), i, j);
    DopeRemapClip3D(a, i, j);
    Push(new ClipOrderUndo(a, i, j, actPrev));
    return true;
}

// ---- BORRADO: la OTRA mitad (borrar tambien corre los indices) ---------------------------
// Ver el bloque (B) de Undo.h: esto es SOLO para las listas cuyo "-" no empuja ningun paso de
// undo. Las que si lo empujan (las cuatro capas de la malla via UndoCaptureMeshGeo, los
// armatures 2D via UndoArm2DDelete, los mesh parts) NO tienen que avisar: su Ctrl+Z restaura
// la lista entera con los indices originales y este desplazamiento los dejaria corridos.
void UndoDeletedList(const W3dRenameDest& list) {
    if (list.i < 0) return;                 // nada que borrar (indice fuera de rango)
    RemapInStacks(list, list.i, -1);        // b < 0 = borrado del indice a
}
void UndoMovedList(const W3dRenameDest& list, int i, int j) {
    if (i == j || i < 0 || j < 0) return;
    RemapInStacks(list, i, j);               // b >= 0 = swap a <-> b
}
// atajos: aviso + borrado en UN solo lugar (ver Undo.h). NO hacen el borrado deshacible: eso
// es una funcionalidad aparte (habria que quedarse con el clip vivo, como UndoArm2DDelete).
// Lo que arreglan es que el rename ANTERIOR no escriba en el clip equivocado.
// ...y el mismo aviso a la SELECCION DEL DOPE, que tambien lleva el indice del clip / de la escena adentro
// de su clave desde la ronda 7 (ver el bloque de identidad en Timeline.cpp).
void UndoDeleteClipArm(Armature* a) {
    if (!a) return;
    const int i = a->activeAnim;
    if (i >= 0 && i < (int)a->animations.size()) { UndoDeletedList(W3dDestClipArm(a, i)); DopeRemapClip3D(a, i, -1); }
    DeleteActiveAnimation(a);
}
void UndoDeleteClip2D(Mesh* m) {
    if (!m) return;
    Armature2D* arm = m->Arm2DActiveP();
    const int i = arm ? m->Arm2DActiveAnim() : -1;
    if (arm && i >= 0 && i < (int)m->Arm2DAnims().size()) {
        UndoDeletedList(W3dDestClip2D(m, arm, i));
        char sa[48]; sprintf(sa, "/a%d/k", m->armature2dActive);
        DopeRemapIndexKey("arm2d:" + DopeIdOwner(m) + sa, i, -1);
    }
    Arm2DDeleteActiveAnimation(m);
}
// ============================================================================
//  ANIMACIONES DE ESCENA (SceneAnimations, lista GLOBAL de la raiz activa) Y CLIPS DE JERARQUIA
//  (animation/W3dAnimSet.h): borrar y crear SON DESHACIBLES.
//
//  El "-" de la tarjeta Animation con una animacion de ESCENA activa la SACA de la lista y el paso de undo se
//  queda con ella (con sus curvas, su camara, su rango): Ctrl+Z la devuelve a SU posicion y la deja activa. Es
//  la familia (1) de Undo.h -como los armatures 2D-: los indices de la lista vuelven a ser los originales antes
//  de que aplique un paso de mas abajo, asi que NO se avisa con UndoDeletedList (desplazarlos los dejaria
//  corridos al reves). La unica animacion de escena no se saca: se VACIA, y el paso guarda sus curvas.
//  La VISTA de un clip de jerarquia (SceneAnimation::isClip) se borra igual y ademas su CLIP sale de la
//  biblioteca (lo dejan de ver todas sus raices): el paso guarda los dos y el Ctrl+Z los devuelve juntos.
//  Las vistas se AGREGAN al final (W3dClipView, sin paso de undo): agregar al final no corre ningun indice
//  de abajo, y el que borra vuelve a dejar la lista como la encontro.
// ============================================================================
// una referencia del HISTORIAL a una biblioteca de clips (W3dAnimSet::refsUndo: no es un usuario)
static void LibUndoRetain(W3dResource* r) {
    if (!r) return;
    W3dResourceRetain(r, W3DREC_PERMANENT);
    W3dAnimSet* s = W3dHierLibrary(r);
    if (s) s->refsUndo++;
}
static void LibUndoRelease(W3dResource* r) {
    if (!r) return;
    W3dAnimSet* s = W3dHierLibrary(r);
    if (s && s->refsUndo > 0) s->refsUndo--;
    W3dResourceRelease(r, W3DREC_PERMANENT);   // (con la ultima referencia, la biblioteca se libera)
}
// el timeline vuelve a una animacion de ESCENA (la que el paso acaba de devolver o la que quedo)
static void AnimPickedScene() {
    if (ActiveAnimKind == 2) AnimIsGame = false;
    ActiveAnimKind = 0; ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    AnimLoadActiveRange();
    g_redraw = true;
}
static int SceneIndex(const SceneAnimation* e) {
    for (size_t i = 0; i < SceneAnimations.size(); i++) if (SceneAnimations[i] == e) return (int)i;
    return -1;
}
class SceneAnimListUndo : public UndoCmd {
    SceneAnimation* e;     // la animacion (mientras esta AFUERA de la lista es del paso: la libera el destructor)
    int  pos;              // su posicion en SceneAnimations mientras esta afuera
    bool inside;           // esta AHORA en la lista?
    bool empty;            // la UNICA animacion de escena: no sale, se vacian/devuelven sus curvas
    std::vector<AnimationObject> curves;   // (vaciar) las curvas guardadas
    W3dResource* rec;       // la vista de un clip de jerarquia: la biblioteca de su clip (referencia del paso)
    W3dClipHier* clip;      // ...y el clip, que sale y vuelve con ella (afuera es del paso)
    int  posClip;
    static void ReleaseObjects(std::vector<AnimationObject>& v, Object* deleted) {
        for (size_t i = v.size(); i-- > 0; ) if (v[i].obj == deleted) v.erase(v.begin() + (long)i);
    }
public:
    // 'dentro' = la animacion esta en la lista (se acaba de CREAR: Ctrl+Z la saca); si no, ya se saco (borrado)
    SceneAnimListUndo(SceneAnimation* E, int P, bool Inside, W3dResource* R, W3dClipHier* C, int PC)
        : e(E), pos(P), inside(Inside), empty(false), rec(R), clip(C), posClip(PC) { LibUndoRetain(rec); }
    // la UNICA escena vaciada: 'c' = sus curvas de antes
    SceneAnimListUndo(SceneAnimation* E, std::vector<AnimationObject>& c)
        : e(E), pos(-1), inside(true), empty(true), rec(0), clip(0), posClip(-1) { curves.swap(c); }
    ~SceneAnimListUndo() {
        if (!inside && !empty) { delete e; if (clip && !W3dHierClipLive(rec, clip, clip->series)) delete clip; }
        LibUndoRelease(rec);
    }
    void Apply() {
        InitSceneAnimations();
        if (empty) {
            const int i = SceneIndex(e);
            if (i < 0) return;
            std::vector<AnimationObject>& live = (i == ActiveSceneAnim) ? AnimationObjects : e->objects;
            live.swap(curves);
            W3dAnimCurvesInvalidate();
            g_redraw = true;
            return;
        }
        if (inside) {                       // SACARLA (el redo de un borrado / el undo de una creacion)
            const int i = SceneIndex(e);
            if (i < 0) return;
            if (i != ActiveSceneAnim) SetActiveScene(i);
            if (clip) { W3dClipsViewsSync(); posClip = W3dHierClipRemove(rec, clip); }
            int p = -1;
            SceneAnimation* x = W3dActiveSceneRemove(&p);
            if (x != e) { if (x) W3dSceneReturn(x, p); return; }   // (no deberia pasar)
            pos = p;
            inside = false;
            DopeRemapIndexKey("obj:e", pos, -1);   // la seleccion del dope indexa esta lista
        } else {                            // DEVOLVERLA a su lugar, activa (con su clip)
            if (clip) W3dHierClipReturn(rec, clip, posClip);
            if (pos > (int)SceneAnimations.size()) pos = (int)SceneAnimations.size();
            W3dSceneReturn(e, pos);
            DopeInsertIndexKey("obj:e", pos);
            inside = true;
        }
        AnimPickedScene();
    }
    // (c) pos: posicion en SceneAnimations. La lista es familia (1) (sacar/devolver pasan por este paso), pero
    //     si alguien avisara un corrimiento mientras esta afuera, se sigue. El resto: (a) e/rec/clip (del paso o
    //     validados), (b) curvas (copia por valor), (d) posClip (posicion en la biblioteca, que solo cambia por pasos)
    void RemapList(const W3dRenameDest& list, int a, int b) {
        if (inside || empty || !W3dSameList(W3dDestGlobal(W3dRenameDest::SceneAnimG, -1), list)) return;
        RemapIndexSaved(pos, a, b);
    }
    // un objeto que se LIBERA no puede quedar nombrado en las curvas que guarda el paso (ni ser la raiz de la vista)
    void UnlinkDetached(Object* deleted) {
        if (empty) { ReleaseObjects(curves, deleted); return; }
        if (inside || !e) return;
        ReleaseObjects(e->objects, deleted);
        if (e->owner == deleted) e->owner = 0;
    }
};
void UndoDeleteActiveScene() {
    InitSceneAnimations();
    SceneAnimation* act = SceneAnimations[ActiveSceneAnim];
    // la VISTA de un clip de jerarquia: primero su CLIP sale de la biblioteca (con lo ultimo que se edito)
    W3dResource* rec = NULL; W3dClipHier* clip = NULL; int posClip = -1;
    if (W3dClipViewLive(ActiveSceneAnim)) {
        W3dClipsViewsSync();
        rec = W3dHierResource(act->owner);
        clip = act->clipHier;
        posClip = W3dHierClipRemove(rec, clip);
        if (posClip < 0) { rec = NULL; clip = NULL; }
    }
    int pos = -1;
    SceneAnimation* e = W3dActiveSceneRemove(&pos);
    if (!e) {
        // la UNICA animacion de escena: se vacia (sus curvas quedan en el paso)
        std::vector<AnimationObject> prev;
        prev.swap(AnimationObjects);
        SceneAnimations[ActiveSceneAnim]->objects.clear();
        W3dAnimCurvesInvalidate();
        if (!prev.empty()) Push(new SceneAnimListUndo(act, prev));
        return;
    }
    DopeRemapIndexKey("obj:e", pos, -1);   // las claves del dope llevan la escena adentro (ver Timeline.h)
    Push(new SceneAnimListUndo(e, pos, false, rec, clip, posClip));
}
int UndoNewScene() {
    const int idx = NewScene();
    InitSceneAnimations();
    if (idx >= 0 && idx < (int)SceneAnimations.size())
        Push(new SceneAnimListUndo(SceneAnimations[(size_t)idx], idx, true, NULL, NULL, -1));
    return idx;
}

// ---- LA BIBLIOTECA DE CLIPS DE JERARQUIA de un objeto (el desplegable "Clips"): el paso guarda la OTRA ----
class HierLibraryUndo : public UndoCmd {
    Object* o;               // (a) el gancho del destructor lo pone en NULL
    W3dResource* rec;        // (a) la otra biblioteca, con una referencia del paso (no se libera mientras exista)
    std::string animset, notLoaded;   // (d)
public:
    HierLibraryUndo(Object* O) : o(O), rec(W3dHierResource(O)) {
        LibUndoRetain(rec);
        if (O && O->clipsHier) { animset = O->clipsHier->animset; notLoaded = O->clipsHier->notLoaded; }
    }
    ~HierLibraryUndo() { LibUndoRelease(rec); }
    void Apply() {
        if (!o) return;
        W3dClipsViewsSync();   // (la vista que se edita va a su clip antes de que la raiz cambie)
        W3dResource* cur = W3dHierResource(o);
        LibUndoRetain(cur);           // (antes de soltarla: puede ser la unica referencia)
        const std::string curN = o->clipsHier ? o->clipsHier->animset : std::string();
        const std::string curNC = o->clipsHier ? o->clipsHier->notLoaded : std::string();
        W3dHierRestore(o, rec, animset, notLoaded);
        LibUndoRelease(rec);
        rec = cur; animset = curN; notLoaded = curNC;
        W3dClipsActiveViewCheck();  // (una vista de la biblioteca que se fue queda huerfana: no se edita)
        W3dResourcesViewInvalidate();
        g_redraw = true;
    }
    W3D_UNDO_NO_INDICES   // o: (a); rec: (a) retenido; animset/notLoaded: (d)
    void UnlinkDetached(Object* deleted) { if (o == deleted) o = NULL; }
};
void UndoCaptureHierLibrary(Object* o) {
    if (!o) return;
    Push(new HierLibraryUndo(o));
}

// ---- EL RETARGET PROPIO de un objeto (armature: el de sus clips de esqueleto; si no, el de sus clips de
//      jerarquia, -1 = el de cada clip): el paso guarda el OTRO valor ----
static int RetargetOf(Object* o) {   // (getType no es const)
    if (!o) return -1;
    if (o->getType() == ObjectType::armature) return ((Armature*)o)->retarget;
    return o->clipsHier ? o->clipsHier->retarget : -1;
}
static void RetargetSet(Object* o, int v) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) { W3dArmatureRetarget((Armature*)o, v < 0 ? W3D_RETARGET_COMPLETE : v); InvalidateArmatureSkin((Armature*)o); }
    else {
        W3dAnimObjectRetarget(o, v);
        if (v < 0 && o->clipsHier && o->clipsHier->animset.empty() && o->clipsHier->notLoaded.empty() && o->clipsHier->layers.empty())
            W3dHierRelease(o);   // (nada que recordar: sin W3dHierRoot, como antes)
    }
}
class RetargetObjUndo : public UndoCmd {
    Object* o;   // (a)
    int value;   // (d) el otro modo
public:
    RetargetObjUndo(Object* O) : o(O), value(RetargetOf(O)) {}
    void Apply() {
        if (!o) return;
        const int cur = RetargetOf(o);
        RetargetSet(o, value);
        value = cur;
        g_redraw = true;
    }
    W3D_UNDO_NO_INDICES   // o: (a); valor: (d)
    void UnlinkDetached(Object* deleted) { if (o == deleted) o = NULL; }
};
void UndoCaptureRetarget(Object* o) {
    if (!o) return;
    Push(new RetargetObjUndo(o));
}

// ---- UN CLIP DE JERARQUIA ENTRA O SALE DE SU BIBLIOTECA (crear / borrar): mismo esquema que los armatures 2D ----
class ClipHierListUndo : public UndoCmd {
    W3dResource* rec;   // (a) su biblioteca (referencia del paso)
    W3dClipHier* clip;  // (a) mientras esta AFUERA es del paso (lo libera el destructor)
    int pos;            // (d) su posicion en la biblioteca mientras esta afuera (se acota al devolverlo)
    bool inside;
    Object* owner;    // (a) CREAR: la raiz en cuya vista se creo (el Ctrl+Y la vuelve a elegir); el gancho la pone en NULL
public:
    ClipHierListUndo(W3dResource* R, W3dClipHier* C, int P, bool Inside, Object* D = NULL)
        : rec(R), clip(C), pos(P), inside(Inside), owner(D) { LibUndoRetain(rec); }
    ~ClipHierListUndo() {
        if (!inside && clip && !W3dHierClipLive(rec, clip, clip->series)) delete clip;   // (afuera es del paso)
        LibUndoRelease(rec);
    }
    void Apply() {
        if (!rec || !clip) return;
        if (inside) {
            W3dClipsViewsSync();              // (lo ultimo que se edito en su vista sale con el)
            const int p = W3dHierClipRemove(rec, clip);
            if (p < 0) return;
            pos = p; inside = false;
            W3dClipsActiveViewCheck();             // (su vista, si era la activa, deja de editarse)
        } else {
            if (!W3dHierClipReturn(rec, clip, pos)) return;
            inside = true;
            // el Ctrl+Y de CREAR deja el clip ELEGIDO, como quedo al crearlo (el Ctrl+Z habia pasado el timeline a
            // una animacion de escena): solo si se sigue editando animaciones de escena de la misma raiz activa
            // (no en "Juego" ni en el Mix) y la raiz sigue usando esa biblioteca (el paso de la biblioteca, en el
            // mismo grupo, ya se la devolvio)
            if (owner && W3dHierResource(owner) == rec && W3dRootOf(owner) == SceneCollection &&
                ActiveAnimKind == 0 && !g_animMix) {
                const int idx = W3dClipView(owner, clip);
                if (idx >= 0) { SetActiveScene(idx); AnimPickedScene(); }
            }
        }
        W3dResourcesViewInvalidate();
        g_redraw = true;
    }
    W3D_UNDO_NO_INDICES   // rec/clip: (a) del paso o validados; pos: (d) posicion en la BIBLIOTECA (no en una lista con destinos)
    void UnlinkDetached(Object* deleted) { if (owner == deleted) owner = NULL; }
};
void UndoClipAddedHier(W3dResource* r, W3dClipHier* c, Object* owner) {
    if (!r || !c) return;
    const W3dAnimSet* set = W3dHierLibrary(r);
    if (!set) return;
    int pos = -1;
    for (size_t i = 0; i < set->data.hierarchies.size(); i++) if (set->data.hierarchies[i] == c) pos = (int)i;
    if (pos < 0) return;
    Push(new ClipHierListUndo(r, c, pos, true, owner));
}
bool UndoClipHierDelete(W3dResource* r, W3dClipHier* c) {
    W3dClipsViewsSync();
    const int pos = W3dHierClipRemove(r, c);
    if (pos < 0) return false;
    Push(new ClipHierListUndo(r, c, pos, false));
    W3dClipsActiveViewCheck();
    W3dResourcesViewInvalidate();
    return true;
}

// ---- EL NOMBRE Y EL RETARGET POR DEFECTO de un clip de jerarquia (el outliner, la tarjeta Animation) ----
class ClipHierValueUndo : public UndoCmd {
    W3dResource* rec;   // (a) su biblioteca (referencia del paso)
    W3dClipHier* clip;  // (a) validado con su serie (W3dHierClipLive)
    unsigned series;    // (d)
    std::string name;   // (d) el otro nombre
    int retarget;       // (d) el otro modo
public:
    ClipHierValueUndo(W3dResource* R, W3dClipHier* C) : rec(R), clip(C), series(C ? C->series : 0),
        name(C ? C->name : std::string()), retarget(C ? C->retarget : 0) { LibUndoRetain(rec); }
    ~ClipHierValueUndo() { LibUndoRelease(rec); }
    void Apply() {
        if (!W3dHierClipLive(rec, clip, series)) return;
        W3dClipsViewsSync();
        const std::string n = clip->name;
        const int m = clip->retarget;
        W3dHierClipRename(W3dHierLibrary(rec), clip, name);
        if (clip->retarget != retarget) { clip->retarget = retarget; clip->version++; }
        name = n; retarget = m;
        W3dClipActiveViewReread(clip);   // (la vista que se edita toma el nombre de su clip)
        W3dResourcesViewInvalidate();
        g_redraw = true;
    }
    W3D_UNDO_NO_INDICES   // rec/clip: (a); serie/nombre/retarget: (d)
};
void UndoCaptureClipHier(W3dResource* r, W3dClipHier* c) {
    if (!r || !c) return;
    Push(new ClipHierValueUndo(r, c));
}

// ============================================================================
//  JOIN (Ctrl+J): comando ATOMICO = geometria del activo (MeshGeoUndo) + borrado de los mergeados (DeleteUndo).
//  Un solo Ctrl+Z restaura la geo del activo Y re-inserta los objetos mergeados (sin estado intermedio roto).
// ============================================================================
class JoinUndo : public ContainerUndo {
    MeshGeoUndo* geo; DeleteUndo* of;
protected:
    // contenedor: no guarda indices PROPIOS, pero sus partes SI (el MeshGeoUndo lleva el
    // paralelo a las vertex anims, el DeleteUndo es DUENO de las mallas mergeadas) -> todos
    // los virtuales se reenvian, y eso lo hace ContainerUndo con esta lista.
    // Aplicar: dos toggles independientes (geo del activo / arbol de escena), en este orden.
    void Parts(std::vector<UndoCmd*>& out){ out.push_back(geo); out.push_back(of); }
public:
    JoinUndo(MeshGeoUndo* g, DeleteUndo* d) : geo(g), of(d) {}
    ~JoinUndo() { delete geo; delete of; }
};
static MeshGeoUndo* g_pendingJoin = NULL; // geo del activo capturada por UndoJoinStart (antes del merge)

void UndoJoinStart(Mesh* activeMesh) {
    delete g_pendingJoin;
    g_pendingJoin = activeMesh ? new MeshGeoUndo(activeMesh) : NULL;
}
void UndoJoinConfirm() {
    if (!g_pendingJoin) return;
    // los objetos a borrar son los que quedaron con select=true (el caller dejo SOLO los mergeados marcados).
    DeleteUndo* of = new DeleteUndo(false); // detacha (sin liberar) + guarda para deshacer
    Push(new JoinUndo(g_pendingJoin, of));  // 1 comando atomico (geo + borrado)
    g_pendingJoin = NULL;                   // ahora lo posee el JoinUndo
}

// ARMATURE: snapshot de la REST (T/R/S/preRot/rotOrder/tlNode/head/tail/pose) de todos los huesos, para deshacer un
// Apply Transform sobre el armature (que la hornea). Apply() = SWAP + re-skinnea las mallas del rig.
struct BoneRestEst { Vector3 restT, restR, restS, preRot, head, tail, poseT, poseR, poseS; int rotOrder; Matrix4 tlNode; };
class ArmatureBonesUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)a == deleted) a = NULL; }
private:
    Armature* a; std::vector<BoneRestEst> e;
public:
    ArmatureBonesUndo(Armature* arm) : a(arm) {
        if (!a) return;
        for (size_t i=0;i<a->bones.size();i++){ W3dBone& b=a->bones[i];
            BoneRestEst t; t.restT=b.restT; t.restR=b.restR; t.restS=b.restS; t.preRot=b.preRot; t.rotOrder=b.rotOrder;
            t.tlNode=b.tlNode; t.head=b.head; t.tail=b.tail; t.poseT=b.poseT; t.poseR=b.poseR; t.poseS=b.poseS;
            e.push_back(t); }
    }
    bool IsEmpty() const { return e.empty(); }
    void Apply() {
        if (!a || e.size()!=a->bones.size()) return;
        for (size_t i=0;i<e.size();i++){ W3dBone& b=a->bones[i]; BoneRestEst& t=e[i];
            Vector3 rt=b.restT,rr=b.restR,rs=b.restS,pr=b.preRot,hd=b.head,tl=b.tail,pt=b.poseT,pR=b.poseR,pS=b.poseS; int ro=b.rotOrder; Matrix4 tn=b.tlNode;
            b.restT=t.restT; b.restR=t.restR; b.restS=t.restS; b.preRot=t.preRot; b.rotOrder=t.rotOrder; b.tlNode=t.tlNode;
            b.head=t.head; b.tail=t.tail; b.poseT=t.poseT; b.poseR=t.poseR; b.poseS=t.poseS;
            t.restT=rt; t.restR=rr; t.restS=rs; t.preRot=pr; t.rotOrder=ro; t.tlNode=tn; t.head=hd; t.tail=tl; t.poseT=pt; t.poseR=pR; t.poseS=pS; }
        a->poseSerial++; // la pose se recalcula (rest nueva)
        InvalidateArmatureSkin(a); // fuerza re-skin con la rest nueva
    }
    // e[] es (c): vector PARALELO a Armature::bones. Esa lista es familia (1) -TODA edicion de
    // huesos empuja un BonesUndo, que la restaura entera, asi que por LIFO los indices vuelven
    // solos- y ademas Aplicar tiene guard de tamano (e.size()!=bones.size() -> no-op). Nadie
    // llama RemapInStacks con la lista de huesos; si algun dia pasara, hay que remapear aca.
    W3D_UNDO_NO_INDICES
};

// ============================================================================
//  APPLY (Alt+A): comando ATOMICO = geometria (MeshGeoUndo por malla) + transform (TransformUndo de los
//  seleccionados) + rest de huesos (ArmatureBonesUndo por armature). Un Ctrl+Z restaura la geo horneada, los
//  pos/rot/scale reseteados Y la rest de los huesos en 1 solo paso.
// ============================================================================
class ApplyUndo : public ContainerUndo {
    std::vector<MeshGeoUndo*> geos; TransformUndo* xf; std::vector<ArmatureBonesUndo*> arms;
protected:
    // contenedor: PROPAGA TODOS los virtuales via ContainerUndo (los MeshGeoUndo de adentro
    // llevan el paralelo a las vertex anims). Orden de Aplicar: geos, transform, rest de huesos
    // (son toggles independientes).
    void Parts(std::vector<UndoCmd*>& out){
        for (size_t i=0;i<geos.size();i++) out.push_back(geos[i]);
        out.push_back(xf);
        for (size_t i=0;i<arms.size();i++) out.push_back(arms[i]);
    }
public:
    ApplyUndo(const std::vector<MeshGeoUndo*>& g, TransformUndo* x, const std::vector<ArmatureBonesUndo*>& ar) : geos(g), xf(x), arms(ar) {}
    ~ApplyUndo() { for (size_t i=0;i<geos.size();i++) delete geos[i]; delete xf; for (size_t i=0;i<arms.size();i++) delete arms[i]; }
};
static std::vector<MeshGeoUndo*> g_pendingApplyGeos;
static TransformUndo* g_pendingApplyXf = NULL;
static std::vector<ArmatureBonesUndo*> g_pendingApplyArms;

void UndoApplyStart() {
    for (size_t i=0;i<g_pendingApplyGeos.size();i++) delete g_pendingApplyGeos[i];
    g_pendingApplyGeos.clear();
    for (size_t i=0;i<g_pendingApplyArms.size();i++) delete g_pendingApplyArms[i];
    g_pendingApplyArms.clear();
    delete g_pendingApplyXf; g_pendingApplyXf = new TransformUndo(); // snapshot de pos/rot/scale de los seleccionados
    for (size_t i=0;i<ObjSelects.size();i++){ Object* o=ObjSelects[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh) g_pendingApplyGeos.push_back(new MeshGeoUndo((Mesh*)o));
        else if (o->getType()==ObjectType::armature) g_pendingApplyArms.push_back(new ArmatureBonesUndo((Armature*)o)); }
}
void UndoApplyConfirm() {
    if (g_pendingApplyGeos.empty() && g_pendingApplyArms.empty() && !g_pendingApplyXf) return;
    Push(new ApplyUndo(g_pendingApplyGeos, g_pendingApplyXf, g_pendingApplyArms)); // 1 comando (geo + transform + rest)
    g_pendingApplyGeos.clear(); g_pendingApplyXf = NULL; g_pendingApplyArms.clear();
}

// ============================================================================
//  POSE (G/R/S de huesos en Pose Mode): snapshot de la POSE (poseT/R/S) de todos los huesos antes del transform.
//  Ctrl+Z restaura la pose previa. La pose se guarda a la curva recien con Insert Keyframe (esto solo deshace el drag).
// ============================================================================
struct PoseEst { Vector3 T, R, S; };
class PoseUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)a == deleted) a = NULL; }
private:
    Armature* a; std::vector<PoseEst> e;
public:
    PoseUndo(Armature* arm) : a(arm) {
        if (!a) return;
        for (size_t i=0;i<a->bones.size();i++){ PoseEst p; p.T=a->bones[i].poseT; p.R=a->bones[i].poseR; p.S=a->bones[i].poseS; e.push_back(p); }
    }
    bool IsEmpty() const { return e.empty(); }
    bool Change() const { // hubo cambio real respecto al snapshot? (evita empujar un undo vacio si G+Esc / click sin mover)
        if (!a || e.size()!=a->bones.size()) return false;
        for (size_t i=0;i<e.size();i++){ const W3dBone& b=a->bones[i];
            if (b.poseT.x!=e[i].T.x||b.poseT.y!=e[i].T.y||b.poseT.z!=e[i].T.z) return true;
            if (b.poseR.x!=e[i].R.x||b.poseR.y!=e[i].R.y||b.poseR.z!=e[i].R.z) return true;
            if (b.poseS.x!=e[i].S.x||b.poseS.y!=e[i].S.y||b.poseS.z!=e[i].S.z) return true; }
        return false;
    }
    void Apply(){
        if (!a || e.size()!=a->bones.size()) return;
        for (size_t i=0;i<e.size();i++){ W3dBone& b=a->bones[i]; Vector3 t=b.poseT,r=b.poseR,s=b.poseS;
            b.poseT=e[i].T; b.poseR=e[i].R; b.poseS=e[i].S; e[i].T=t; e[i].R=r; e[i].S=s; }
        a->poseDirty=true; a->poseSerial++; InvalidateArmatureSkin(a);
    }
    W3D_UNDO_NO_INDICES // e[]: (c) paralelo a Armature::bones, mismo caso que ArmatureBonesUndo
                         // (familia (1) + guard de tamano). a: (a) Armature*.
};
// ============================================================================
//  KEYFRAMES (dope sheet / editor de curvas / tarjeta "Keyframe"): snapshot de TODAS las curvas
//  de la animacion que se esta editando (escena, clip 3D, clip del armature 2D o vertex anim de
//  objeto). Ctrl+Z devuelve los keyframes.
//
//  *** LA LISTA VA POR IDENTIDAD, NO "LA QUE ESTE ACTIVA AL DESHACER" (fallo (5)) ***
//  Antes el snapshot era un vector PARALELO POSICIONAL y la lista de curvas se RE-RESOLVIA AL
//  APLICAR desde los GLOBALES (ActiveAnimKind / ActiveAnimArm->activeAnim / currentAnim /
//  AnimationObjects), con un unico guard "c.size() == snap.size()" -que es justo lo que Undo.h
//  llama "la version silenciosa del bug"-. O sea: el Ctrl+Z escribia en la lista que estuviera
//  activa EN ESE MOMENTO, no en la que se edito. Tres caminos de UI normales lo rompian, los
//  tres sin crash y sin aviso:
//    - editar un keyframe del clip K0 y despues elegir K1 en la tarjeta Animation (activeAnim=1;
//      dos clips de igual estructura pasan el guard): el Ctrl+Z metia las curvas de K0 en K1;
//    - editar el key de un objeto, BORRAR ese objeto y deshacer el borrado: AnimationObjects
//      volvia en otro orden y las curvas quedaban CRUZADAS entre objetos;
//    - editar en la escena 1 y elegir la escena 0 (SetActiveScene hace swap de TODA la lista):
//      el Ctrl+Z escribia las curvas de una escena ENCIMA de las de la otra.
//  Ahora el paso guarda DOS identidades y no mira ningun conteo:
//    (1) DE QUE LISTA salio: kind + dueno concreto + cual clip/anim/escena (KFList). Se resuelve
//        ESA al aplicar; si ya no existe, no-op seguro. El indice de esa lista es un miembro (c)
//        y se remapea (RemapList) como cualquier otro.
//    (2) QUE CURVA es cada entrada dentro de la lista (KFId): el objeto (escena) o el hueso del
//        track (clips 3D/2D) + (propiedad, componente). Ni el orden ni la cantidad importan:
//        cada curva recupera LO SUYO, una curva que ya no esta se saltea y una curva NUEVA
//        (la creo la operacion que estamos deshaciendo) se vacia y queda guardada para el redo.
// ============================================================================
extern Mesh* ActiveAnimMesh;   // (kind 3) la malla cuya vertex anim de objeto esta activa

// ---- (1) IDENTIDAD DE LA LISTA DE CURVAS -------------------------------------------------
// 'kind' es el ActiveAnimKind CON EL QUE SE CAPTURO (el 2 -modo juego- usa las curvas de escena,
// igual que el else de la recoleccion, asi que se guarda como 0). 'idx' es la posicion en LA
// lista que corresponde a ese kind, y por eso es un miembro (c) que se remapea:
//    kind 0 -> SceneAnimations           (familia (1): UndoDeleteActiveScene/UndoNewScene la sacan y la devuelven)
//    kind 1 -> Armature::animations      (familia (2): UndoDeleteClipArm / UndoMoveClipArm avisan)
//    kind 3 -> Mesh::animations          (familia (2): el "-" de la vertex anim avisa)
//    kind 4 -> Armature2D::anims         (familia (2): UndoDeleteClip2D avisa)
struct KFList {
    int kind;
    Armature*   arm;     // kind 1
    Mesh*       mesh;    // kind 3 y 4
    Armature2D* arm2d;   // kind 4: el rig 2D CONCRETO (la lista de la malla puede cambiar)
    int idx;
    KFList() : kind(0), arm(0), mesh(0), arm2d(0), idx(-1) {}
};
// ---- (2) IDENTIDAD DE UNA CURVA DENTRO DE LA LISTA ---------------------------------------
// obj  : kind 0, el objeto animado (AnimationObject::obj). NUNCA se desreferencia: solo se
//        compara contra los punteros vivos de la lista.
// bone : kind 1 y 4, el hueso del track (BoneTrack::bone / Bone2DTrack::bone).
// prop/comp: la curva (AnimPosition/AnimRotation/..., AnimX/Y/Z).
// mat  : kind 0, una PISTA DE MATERIAL de la escena (Animation.h: AnimationMaterial; bone = -3): su material. Como
//        'obj', solo se compara (los materiales no se liberan mientras haya undo: MaterialPurge los deja vivos).
// ocur : la k-esima curva de la lista con esos mismos cuatro datos. Un clip puede traer DOS pistas
//        del mismo hueso (un archivo importado; el FK usa la primera): sin esto las dos curvas
//        tenian la MISMA identidad, el Ctrl+Z le devolvia el snapshot a la primera y a la segunda
//        la vaciaba como "curva nueva" (se perdian sus keyframes). Lo numera KFResolve.
struct KFId {
    Object* obj; int bone; int prop, comp; int ocur; const Material* mat;
    KFId() : obj(0), bone(-1), prop(-1), comp(-1), ocur(0), mat(0) {}
    bool operator==(const KFId& o) const {
        return obj == o.obj && bone == o.bone && prop == o.prop && comp == o.comp && ocur == o.ocur && mat == o.mat;
    }
    bool operator<(const KFId& o) const {   // solo los cuatro datos (la clave para numerar 'ocur')
        if (mat != o.mat) return mat < o.mat;
        if (obj != o.obj) return obj < o.obj;
        if (bone != o.bone) return bone < o.bone;
        if (prop != o.prop) return prop < o.prop;
        return comp < o.comp;
    }
};
// (W3dKeys y no std::vector<keyFrame>: capturar, comparar y el swap de Aplicar no expanden las curvas
//  COMPACTAS del clip -Animation.h-; solo se expande la que la operacion edita: claveseditar en
//  tools/pruebas/prueba_claves_compactas.w3s)
struct KFCurve { W3dKeys* c; KFId id; AnimProperty* ap; };   // (ap: el modificador Repetir de la curva)
static void KFAdd(std::vector<KFCurve>& out, std::vector<AnimProperty>& props, Object* obj, int bone){
    for (size_t p = 0; p < props.size(); p++){
        KFCurve k; k.c = &props[p].keyframes; k.ap = &props[p];
        k.id.obj = obj; k.id.bone = bone; k.id.prop = props[p].Property; k.id.comp = props[p].component;
        out.push_back(k);
    }
}
// la lista de curvas que el timeline esta editando AHORA (para capturar)
static void KFActiveIdentity(KFList& id){
    id = KFList();
    if (ActiveAnimKind == 1 && ActiveAnimArm){
        id.kind = 1; id.arm = ActiveAnimArm; id.idx = ActiveAnimArm->activeAnim;
    } else if (ActiveAnimKind == 4 && ActiveAnimMesh){
        id.kind = 4; id.mesh = ActiveAnimMesh; id.arm2d = ActiveAnimMesh->Arm2DActiveP();
        id.idx = id.arm2d ? ActiveAnimMesh->Arm2DActiveAnim() : -1;
    } else if (ActiveAnimKind == 3 && ActiveAnimMesh){
        id.kind = 3; id.mesh = ActiveAnimMesh;
        VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
        id.idx = va ? va->currentAnim : -1;
    } else {                                   // escena (y modo juego, que anima las mismas curvas)
        InitSceneAnimations();
        id.kind = 0; id.idx = ActiveSceneAnim;
    }
}
// resuelve LA LISTA GUARDADA (no la activa) -> sus curvas vivas + la identidad de cada una.
// Sale VACIO si esa lista ya no existe: el paso queda en no-op seguro.
// 'escribir' = el que llama va a escribir las curvas (Aplicar): un clip de esqueleto de un animset
// COMPARTIDO pasa antes a clips propios. Capturar y comparar (Cambio) solo leen: no copian.
static void KFResolve(const KFList& id, std::vector<KFCurve>& out, bool write){
    out.clear();
    if (id.idx < 0) return;
    if (id.kind == 1){
        if (!id.arm || !ObjectInScene(id.arm)) return;
        if (id.idx >= (int)id.arm->animations.size()) return;
        if (write) W3dOwnArmatureAnims(id.arm);   // se van a escribir: nunca las de un clip compartido
        SkeletalAnimation* an = id.arm->animations[id.idx]; if (!an) return;
        for (size_t t = 0; t < an->tracks.size(); t++) KFAdd(out, an->tracks[t].Properties, NULL, an->tracks[t].bone);
    } else if (id.kind == 4){
        if (!id.mesh || !ObjectInScene(id.mesh) || !Arm2DInMesh(id.mesh, id.arm2d)) return;
        if (id.idx >= (int)id.arm2d->anims.size()) return;
        Armature2DAnimation* an = id.arm2d->anims[id.idx]; if (!an) return;
        for (size_t t = 0; t < an->tracks.size(); t++) KFAdd(out, an->tracks[t].Properties, NULL, an->tracks[t].bone);
    } else if (id.kind == 3){
        if (!id.mesh || !ObjectInScene(id.mesh)) return;
        if (id.idx >= (int)id.mesh->animations.size()) return;
        VertexAnimation* an = id.mesh->animations[id.idx]; if (!an) return;
        KFAdd(out, an->curves, NULL, -1);
    } else {
        InitSceneAnimations();
        if (id.idx >= (int)SceneAnimations.size()) return;
        // las curvas de la escena ACTIVA viven en el global AnimationObjects; las de las demas,
        // guardadas en su SceneAnimation (SetActiveScene hace swap entre las dos).
        std::vector<AnimationObject>& lst = (id.idx == ActiveSceneAnim)
                                          ? AnimationObjects : SceneAnimations[id.idx]->objects;
        for (size_t i = 0; i < lst.size(); i++) KFAdd(out, lst[i].Properties, lst[i].obj, -1);
        // la pista "Camara activa" de esa escena (dope: mover / borrar sus cortes -> Ctrl+Z)
        if (SceneAnimations[id.idx]) KFAdd(out, SceneAnimations[id.idx]->camTrack, NULL, -2);
        // las PISTAS DE MATERIAL de esa escena (viven siempre en su SceneAnimation): cada curva por su material
        if (SceneAnimations[id.idx]) {
            std::vector<AnimationMaterial>& mts = SceneAnimations[id.idx]->materials;
            for (size_t i = 0; i < mts.size(); i++) {
                const size_t n0 = out.size();
                KFAdd(out, mts[i].Properties, NULL, -3);
                for (size_t q = n0; q < out.size(); q++) out[q].id.mat = mts[i].mat;
            }
        }
    }
    // las curvas REPETIDAS (misma identidad) se distinguen por su orden en la lista (ver KFId::ocur)
    std::map<KFId, int> views;
    for (size_t i = 0; i < out.size(); i++) out[i].id.ocur = views[out[i].id]++;
}
static int KFFind(const std::vector<KFCurve>& c, const KFId& id){
    for (size_t i = 0; i < c.size(); i++) if (c[i].id == id) return (int)i;
    return -1;
}
class KeyframesUndo : public UndoCmd {
    struct KFSaved { KFId id; W3dKeys kf; int ca, cd; std::vector<KfModifier> mods; KFSaved() : ca(0), cd(0) {} };   // (una curva compacta se guarda compacta; ca/cd = Repetir; mods = su pila de modificadores)
    std::vector<KFSaved> snap;
    KFList lid;                 // de QUE lista de curvas salio el snapshot
    // snapshot de los FRAMES de vertices de la anim de objeto activa (kind 3). Se guarda
    // como BLOB binario (VertexAnimSerialize): es una copia por valor autocontenida, sin
    // gestion manual de los buffers -> imposible doble-free/fuga. Va POR (malla + indice)
    // -que es la MISMA identidad de lista de arriba, lid.mesh/lid.idx- y no por un
    // VertexAnimation* crudo: el "-" de la tarjeta hace delete y el proximo new puede caer en
    // LA MISMA direccion (ver Undo.h). Ese indice es (c) y lo remapea RemapList.
    std::vector<unsigned char> vfBlob; bool vfHas;
    VertexAnimation* VfAnim() const {
        if (lid.kind != 3 || !lid.mesh) return NULL;
        if (lid.idx < 0 || lid.idx >= (int)lid.mesh->animations.size()) return NULL;
        return lid.mesh->animations[lid.idx];
    }
public:
    // arm != NULL: el clip ACTIVO de ese esqueleto aunque el timeline muestre otra cosa (el auto key de pose
    // escribe ahi siempre)
    KeyframesUndo(Armature* arm = NULL){
        KFActiveIdentity(lid);
        if (arm){ lid = KFList(); lid.kind = 1; lid.arm = arm; lid.idx = arm->activeAnim; }
        std::vector<KFCurve> c; KFResolve(lid, c, false);   // capturar solo lee
        snap.resize(c.size());
        for (size_t i = 0; i < c.size(); i++){ snap[i].id = c[i].id; snap[i].kf = *c[i].c;
            snap[i].ca = c[i].ap->cyclePrev; snap[i].cd = c[i].ap->cycleAfter; snap[i].mods = c[i].ap->mods; }
        vfHas = false;
        VertexAnimation* van = VfAnim();
        if (van && van->target){ VertexAnimSerialize(*van, vfBlob); vfHas = true; }
    }
    bool IsEmpty() const { return snap.empty() && !vfHas; }
    bool Change() const { // hubo cambio real? (no empujar un undo vacio)
        std::vector<KFCurve> c; KFResolve(lid, c, false);   // comparar solo lee
        std::vector<char> seen(c.size(), 0);
        for (size_t i = 0; i < snap.size(); i++){
            const int k = KFFind(c, snap[i].id);
            if (k < 0){ if (!snap[i].kf.empty()) return true; continue; } // la curva ya no existe
            seen[k] = 1;
            const W3dKeys& live = *c[k].c; const W3dKeys& g = snap[i].kf;
            if (live.size() != g.size()) return true;
            if (c[k].ap->cyclePrev != snap[i].ca || c[k].ap->cycleAfter != snap[i].cd) return true;   // Repetir
            if (c[k].ap->mods != snap[i].mods) return true;                                                  // modificadores
            for (size_t q = 0; q < g.size(); q++){
                const keyFrame a = live.Key(q); const keyFrame b = g.Key(q);   // (por valor: no expande)
                // OJO: tambien la INTERPOLACION y los HANDLES. Curvar un tramo no mueve el keyframe (mismo frame y
                // mismo valor): si solo se miraban esos dos, el undo de una curva se descartaba por "no hubo cambio".
                if (a.frame != b.frame || a.value != b.value) return true;
                if (a.Interpolation != b.Interpolation || a.handleType != b.handleType) return true;
                if (a.inDF != b.inDF || a.inDV != b.inDV || a.outDF != b.outDF || a.outDV != b.outDV) return true;
            }
        }
        for (size_t k = 0; k < c.size(); k++)      // curva NUEVA con keyframes (la creo la operacion)
            if (!seen[k] && !c[k].c->empty()) return true;
        // FRAMES de vertices: comparar el blob de ahora contra el snapshot (asi un cambio que
        // fue SOLO de vertices -mover/borrar/interp/insertar- tambien empuja undo)
        if (vfHas){ VertexAnimation* v = VfAnim();
            if (v && v->target){ std::vector<unsigned char> now; VertexAnimSerialize(*v, now); if (now != vfBlob) return true; } }
        return false;
    }
    void Apply(){
        std::vector<KFCurve> c; KFResolve(lid, c, true);   // LA lista guardada, no la activa (se escribe)
        std::vector<char> seen(c.size(), 0);
        for (size_t i = 0; i < snap.size(); i++){
            const int k = KFFind(c, snap[i].id);
            if (k < 0) continue;                      // esa curva ya no existe: la entrada queda guardada
            seen[k] = 1;
            c[k].c->swap(snap[i].kf);                 // swap = sirve de ida (undo) y de vuelta (redo)
            { int t = c[k].ap->cyclePrev;   c[k].ap->cyclePrev   = snap[i].ca; snap[i].ca = t;
              t = c[k].ap->cycleAfter;     c[k].ap->cycleAfter = snap[i].cd; snap[i].cd = t;
              c[k].ap->mods.swap(snap[i].mods); }
        }
        // curvas que NO estaban al capturar (las creo la operacion que se esta deshaciendo): se
        // vacian y su contenido queda en el snapshot, asi el redo las devuelve enteras.
        for (size_t k = 0; k < c.size(); k++){
            if (seen[k] || c[k].c->empty()) continue;
            KFSaved g; g.id = c[k].id; c[k].c->swap(g.kf);
            g.ca = c[k].ap->cyclePrev; g.cd = c[k].ap->cycleAfter; c[k].ap->cyclePrev = c[k].ap->cycleAfter = 0;
            g.mods.swap(c[k].ap->mods);
            snap.push_back(g);
        }
        // la pose cacheada del rig que TOCAMOS (no la del que este activo) tiene que re-evaluarse
        if (lid.kind == 1 && lid.arm){ lid.arm->lastPoseFrame = -999999; lid.arm->poseDirty = true; lid.arm->poseSerial++;
                                       InvalidateArmatureSkin(lid.arm); }
        if (ActiveAnimArm && ActiveAnimArm != lid.arm){ ActiveAnimArm->lastPoseFrame = -999999; ActiveAnimArm->poseDirty = true;
                                       ActiveAnimArm->poseSerial++; InvalidateArmatureSkin(ActiveAnimArm); }
        if (lid.kind == 4 && lid.mesh){ lid.mesh->pose2dDirty = true; lid.mesh->lastSkinFrame = -999999; }
        // la VISTA de un clip de jerarquia (animation/Animation.h): lo deshecho va tambien a su clip, que es lo
        // que reproducen todas sus raices (aunque la vista ya no sea la que se esta editando)
        if (lid.kind == 0 && W3dClipViewLive(lid.idx)) W3dClipViewWrite(lid.idx);
        // FRAMES de vertices: swap por serializacion (Deserializar libera lo vivo y reconstruye).
        // Si la malla se re-modelo (vertexSize distinto) Deserializar devuelve false y no toca nada.
        if (vfHas){ VertexAnimation* v = VfAnim();
            if (v && v->target){
                std::vector<unsigned char> now; VertexAnimSerialize(*v, now);       // lo VIVO
                if (VertexAnimDeserialize(*v, vfBlob.empty()?0:&vfBlob[0], vfBlob.size()))
                    vfBlob.swap(now);                                                 // queda con lo vivo (redo)
                extern int CurrentFrame;
                EvalVertexAnim(*v, v->target, (float)CurrentFrame); // que el viewport muestre YA la pose restaurada
                v->target->skinGeomVersion++;                       // re-subir el VBO (sino queda congelado)
            } }
    }
    // (c) lid.idx: POSICION en LA lista de su kind (ver KFList). Es el UNICO miembro posicional:
    //     el snapshot ya no es paralelo (cada entrada lleva su KFId) y el blob de vertices va por
    //     esa misma identidad de lista.
    // (a) lid.arm / lid.mesh / lid.arm2d: punteros revalidados al resolver (ObjectInScene /
    //     Arm2DInMesh). KFId::obj se COMPARA contra los vivos, nunca se desreferencia.
    // (d) lid.kind; (b) snap[].kf y vfBlob (copias por valor, autocontenidas).
    void RemapList(const W3dRenameDest& list, int a, int b) {
        W3dRenameDest mine;
        if      (lid.kind == 1) mine = W3dDestClipArm(lid.arm, -1);
        else if (lid.kind == 3) mine = W3dDestLayerMesh(lid.mesh, W3dRenameDest::VertAnim, -1);
        else if (lid.kind == 4) mine = W3dDestClip2D(lid.mesh, lid.arm2d, -1);
        else                    mine = W3dDestGlobal(W3dRenameDest::SceneAnimG, -1);
        if (!W3dSameList(mine, list)) return;
        if (lid.idx < 0) return;                             // el paso ya estaba muerto
        if (!RemapIndexSaved(lid.idx, a, b)) lid.idx = -1; // borraron ESA anim/clip/escena: no-op seguro
    }
};
static KeyframesUndo* g_pendingKeys = NULL;
void UndoKeyframesStart(){ delete g_pendingKeys; g_pendingKeys = new KeyframesUndo(); }
void UndoKeyframesStartArm(Armature* a){ delete g_pendingKeys; g_pendingKeys = new KeyframesUndo(a); }
void UndoKeyframesConfirm(){
    if (!g_pendingKeys) return;
    // (sin IsEmpty(): el PRIMER keyframe de una escena sin curvas arranca con el snapshot vacio y si cambio -las curvas
    //  nuevas las detecta Cambio-; descartarlo dejaba ese keyframe sin Ctrl+Z)
    if (!g_pendingKeys->Change()){ delete g_pendingKeys; g_pendingKeys = NULL; return; }
    Push(g_pendingKeys); g_pendingKeys = NULL;
}

// ============================================================================
//  EDIT MODE de HUESOS (extrude/duplicar/borrar/mover head-tail/rename): snapshot COMPLETO de bones[]
//  + activeBone + los TRACKS de cada clip (referencian huesos por INDICE: borrar un hueso los remapea,
//  el undo tiene que devolverlos). Apply() = SWAP total + re-preparar el skin autorado (el rest de un
//  rig autorado se deriva de head/tail -> restaurar los huesos exige recomputar las matrices de skin).
// ============================================================================
class BonesUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)a == deleted) a = NULL; }
private:
    Armature* a;
    std::vector<W3dBone> bones;
    int activeBone;
    // TRACKS de los clips 3D. Antes era un vector PARALELO POSICIONAL a a->animations, con el
    // unico guard "tracks.size() == animations.size()" -> los clips son familia (2) (el "-" de la
    // tarjeta Animation borra un clip sin empujar ningun paso de undo) y eso rompia de DOS formas
    // distintas, las dos mudas:
    //   - borrar un clip de ABAJO y crear otro (el conteo vuelve a coincidir): el Ctrl+Z metia
    //     las curvas de cada clip en el clip de al lado, y las del borrado en el clip NUEVO;
    //   - borrar sin crear (el conteo NO coincide): el swap se salteaba ENTERO y el Ctrl+Z no
    //     devolvia NINGUNA curva.
    // Ahora cada entrada LLEVA SU INDICE y se remapea (RemapList). Sin conteos: cada clip
    // recupera lo suyo, el borrado pierde su entrada (idx = -1) y un clip nuevo no recibe nada.
    struct ClipTracks { int idx; std::vector<BoneTrack> tracks; ClipTracks() : idx(-1) {} };
    std::vector<ClipTracks> tracks;
    void Capture(){
        bones = a->bones; activeBone = a->activeBone;
        tracks.clear();
        tracks.resize(a->animations.size());
        for (size_t i = 0; i < a->animations.size(); i++){
            tracks[i].idx = (int)i;
            if (a->animations[i]) tracks[i].tracks = a->animations[i]->tracks;
        }
    }
public:
    BonesUndo(Armature* arm) : a(arm), activeBone(-1) { if (a) Capture(); }
    bool Change() const { // hubo cambio real? (un click que no movio nada no ensucia el stack)
        if (!a) return false;
        if (a->activeBone != activeBone) return true;
        if (a->bones.size() != bones.size()) return true;
        for (size_t i = 0; i < bones.size(); i++){
            const W3dBone& v = a->bones[i]; const W3dBone& s = bones[i];
            if (v.name != s.name || v.parent != s.parent || v.connected != s.connected) return true;
            if (v.head.x!=s.head.x || v.head.y!=s.head.y || v.head.z!=s.head.z) return true;
            if (v.tail.x!=s.tail.x || v.tail.y!=s.tail.y || v.tail.z!=s.tail.z) return true;
            if (v.select != s.select) return true;
        }
        return false;
    }
    void Apply(){
        if (!a) return;
        W3dOwnArmatureAnims(a);   // devuelve las pistas A LOS CLIPS: nunca a unos compartidos
        a->bones.swap(bones);
        int cur = a->activeBone; a->activeBone = activeBone; activeBone = cur;
        // cada entrada vuelve A SU CLIP (por indice remapeado), sin comparar conteos: si el
        // clip se borro la entrada esta muerta (idx = -1) y si hay clips NUEVOS no se tocan.
        for (size_t i = 0; i < tracks.size(); i++){
            const int k = tracks[i].idx;
            if (k < 0 || k >= (int)a->animations.size() || !a->animations[k]) continue;
            a->animations[k]->tracks.swap(tracks[i].tracks);
        }
        // el rest de un rig AUTORADO sale de head/tail -> recomputar matrices de skin con lo restaurado
        if (a->skinAuthored) PrepareAuthoredSkin(a);
        a->poseDirty = false; a->lastPoseFrame = -999999; a->poseSerial++;
        InvalidateArmatureSkin(a); // el skin de las mallas del rig quedo con la rest vieja
    }
    // (c) tracks[].idx: POSICION en Armature::animations (clips 3D), familia (2) -> se remapea.
    //     Cubre las DOS operaciones: borrar un clip (UndoDeleteClipArm) y reordenarlos
    //     (UndoMoveClipArm, que manda el swap a<->b).
    // (b) bones: snapshot COMPLETO de Armature::bones (por eso esa lista es familia (1)).
    // (c) activeBone: posicion en bones, pero vuelve JUNTO con el snapshot completo de arriba.
    // (a) a: Armature*.
    void RemapList(const W3dRenameDest& list, int x, int y) {
        if (!W3dSameList(W3dDestClipArm(a, -1), list)) return;
        for (size_t i = 0; i < tracks.size(); i++)
            if (!RemapIndexSaved(tracks[i].idx, x, y)) tracks[i].idx = -1; // borraron ESE clip
    }
};
static BonesUndo* g_pendingBones = NULL;
void UndoBonesCapture(Armature* a){ if (a) Push(new BonesUndo(a)); }
void UndoBonesStart(Armature* a){ delete g_pendingBones; g_pendingBones = a ? new BonesUndo(a) : NULL; }
void UndoBonesConfirm(){
    if (!g_pendingBones) return;
    if (!g_pendingBones->Change()){ delete g_pendingBones; g_pendingBones = NULL; return; }
    Push(g_pendingBones); g_pendingBones = NULL;
}
void UndoBonesCancel(){ delete g_pendingBones; g_pendingBones = NULL; }

static PoseUndo* g_pendingPose = NULL;
void UndoPoseStart(Armature* a){ delete g_pendingPose; g_pendingPose = a ? new PoseUndo(a) : NULL; }
void UndoPoseConfirm(){
    if (!g_pendingPose) return;
    if (g_pendingPose->IsEmpty() || !g_pendingPose->Change()){ delete g_pendingPose; g_pendingPose = NULL; return; }
    Push(g_pendingPose); g_pendingPose = NULL;
}

// ============================================================================
//  PESOS (weight paint): snapshot de LOS DOS GRUPOS completos (vertex groups por
//  control-point + UV groups por corner, con sus indices activos) al EMPEZAR un
//  trazo del pincel; commit al SOLTAR -> UN paso de undo POR TRAZO (no por
//  movimiento de mouse), como el transform. Si el trazo creo el grupo automatico,
//  el undo tambien lo saca (el snapshot es previo). Se guardan las DOS entidades
//  en el mismo comando porque el trazo del 3D toca una y el del UV la otra, y asi
//  hay UN solo camino de undo para "pintar pesos" (venga del viewport que venga).
// ============================================================================
class WeightsUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    std::vector<VertexGroup> groups;   // copia por VALOR (nombre + verts/pesos por control-point)
    std::vector<UVGroup>     uvgrupos; // copia por VALOR (nombre + verts/pesos por corner)
    int activeGroup, uvActiveGroup;
public:
    WeightsUndo(Mesh* M) : m(M), activeGroup(0), uvActiveGroup(-1) {
        if (!m) return;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) groups.push_back(*m->vertexGroups[i]);
        for (size_t i = 0; i < m->uvGroups.size(); i++)     uvgrupos.push_back(*m->uvGroups[i]);
        activeGroup = m->activeGroup; uvActiveGroup = m->uvActiveGroup;
    }
    bool Change() const { // hubo cambio real? (un click que no pinto nada no ensucia el stack)
        if (!m) return false;
        if ((int)groups.size() != (int)m->vertexGroups.size()) return true;
        if ((int)uvgrupos.size() != (int)m->uvGroups.size()) return true;
        if (activeGroup != m->activeGroup || uvActiveGroup != m->uvActiveGroup) return true;
        for (size_t g = 0; g < groups.size(); g++) {
            const VertexGroup* vg = m->vertexGroups[g];
            if (vg->name != groups[g].name) return true;
            if (vg->verts != groups[g].verts || vg->weights != groups[g].weights) return true;
        }
        // UV groups (pincel del editor UV): sin esto el trazo del UV se descartaba como "sin
        // cambios" (los vertex groups no los toca)
        for (size_t g = 0; g < uvgrupos.size(); g++) {
            const UVGroup* ug = m->uvGroups[g];
            if (ug->name != uvgrupos[g].name) return true;
            if (ug->verts != uvgrupos[g].verts || ug->weights != uvgrupos[g].weights) return true;
        }
        return false;
    }
    void Apply() {
        if (!m) return;
        std::vector<VertexGroup> cur; // estado VIVO (para rehacer)
        std::vector<UVGroup>     curUV;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) cur.push_back(*m->vertexGroups[i]);
        for (size_t i = 0; i < m->uvGroups.size(); i++)     curUV.push_back(*m->uvGroups[i]);
        int curAct = m->activeGroup, curActUV = m->uvActiveGroup;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) delete m->vertexGroups[i];
        m->vertexGroups.clear();
        for (size_t i = 0; i < groups.size(); i++) m->vertexGroups.push_back(new VertexGroup(groups[i]));
        for (size_t i = 0; i < m->uvGroups.size(); i++) delete m->uvGroups[i];
        m->uvGroups.clear();
        for (size_t i = 0; i < uvgrupos.size(); i++) m->uvGroups.push_back(new UVGroup(uvgrupos[i]));
        m->activeGroup = activeGroup; m->uvActiveGroup = uvActiveGroup;
        groups.swap(cur); uvgrupos.swap(curUV);
        activeGroup = curAct; uvActiveGroup = curActUV;
        m->skinGeomVersion++;        // el CSR de skinning no hashea los pesos -> invalidar
        m->lastSkinFrame = -999999;  // re-skin con los pesos restaurados
        // SKINNING 2D: los pesos por corner tambien deforman los UV -> con una POSE REAL hay que
        // re-aplicarla para que el undo se vea al toque. Con la pose en identidad NO se toca uv[]
        // (el skinning no hace nada ahi y re-aplicar solo puede pisar la edicion del usuario).
        if (m->HasArm2D() && !m->Armature2DPoseIdentity()) m->Armature2DApply();
    }
    // grupos/uvgrupos: (b) snapshot COMPLETO de Mesh::vertexGroups y Mesh::uvGroups -Aplicar las
    // reemplaza enteras-, asi que activeGroup/uvActiveGroup (posiciones) vuelven coherentes CON
    // la lista que restauran. Las dos son familia (1). m: (a) Mesh*.
    W3D_UNDO_NO_INDICES
};
static WeightsUndo* g_pendingWeights = NULL;
void UndoWeightsStart(Mesh* m){ delete g_pendingWeights; g_pendingWeights = m ? new WeightsUndo(m) : NULL; }
void UndoWeightsConfirm(){
    if (!g_pendingWeights) return;
    if (!g_pendingWeights->Change()){ delete g_pendingWeights; g_pendingWeights = NULL; return; }
    Push(g_pendingWeights); g_pendingWeights = NULL;
}

// ============================================================================
//  TRANSFORM DE UVs (G/R/S del editor UV / tarjeta "Transform UV" del panel).
//  DECISION liviano vs completo (pedido del dueno, documentada):
//   - LIVIANO (UVMapUndo): mover/rotar/escalar UVs de verts o bordes (y de caras SIN split)
//     solo cambia VALORES dentro de mesh->uv -> alcanza con snapshotear la capa uv completa
//     (patron EditMoveUndo: swap con guard de tamano). Viaja tambien uv2dRest (el rest del
//     skinning 2D acompana a la capa: editar UVs a mano lo invalida y se recaptura lazy).
//   - COMPLETO (MeshGeoUndo): el transform de CARA puede hacer SPLIT de esquinas
//     (UVSeparateFacesSel duplica render-verts -> cambia vertexSize, faces3d, posRep,
//     vertCtrlPoint, vertex anims...). El snapshot liviano no puede deshacer eso, asi que
//     el caller pide IniciarCompleto ANTES del split y se reusa MeshGeoUndo tal cual.
//  Pendiente hasta confirmar: cancelar (o confirmar sin cambio real) descarta y no ensucia
//  el stack. 'cambio' lo decide el caller comparando los uv vivos contra la base del
//  transform: un split SIN mover no pushea (los duplicados identicos se re-mergean solos
//  en el proximo GenerateRender, ver UVSeparateFacesSel).
// ============================================================================
class UVMapUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    std::vector<GLfloat> uv;      // capa mesh->uv completa (2 floats por render-vert)
    std::vector<GLfloat> rest2d;  // uv2dRest (rest del skinning 2D): acompana a la capa
public:
    UVMapUndo(Mesh* M) : m(M) {
        if (m && m->uv && m->vertexSize > 0) uv.assign(m->uv, m->uv + m->vertexSize * 2);
        if (m) rest2d = m->uv2dRest;
    }
    bool IsEmpty() const { return uv.empty(); }
    void Apply() {
        if (!m || !m->uv) return;
        if ((int)uv.size() != m->vertexSize * 2) return; // la topologia cambio: no tocar (robusto)
        m->UninstantiateData(W3DMD_UV);   // COW: se intercambia en el lugar (recurso compartido)
        for (size_t i = 0; i < uv.size(); i++) { GLfloat c = m->uv[i]; m->uv[i] = uv[i]; uv[i] = c; }
        rest2d.swap(m->uv2dRest);  // puede quedar vacio: se recaptura lazy al posar
        m->skinGeomVersion++;      // re-subir el VBO de uv (sino el viewport muestra el mapeo viejo)
        W3dMeshMarkEdited(m);
    }
    W3D_UNDO_NO_INDICES // uv/rest2d: (b) snapshot completo de los arrays (con guard de tamano); m: (a)
};
static UVMapUndo*   g_pendingUV    = NULL; // transform de UV liviano en curso
static MeshGeoUndo* g_pendingUVGeo = NULL; // transform de cara CON split en curso (snapshot total)

// ============================================================================
//  VERTEX COLOR (pincel de color): un paso por TRAZO, igual que los pesos. Pintar solo cambia
//  VALORES de la capa activa (color + indices de paleta), no la topologia -> alcanza el patron
//  LIVIANO de UVMapUndo: snapshot de la capa al empezar el trazo y swap al deshacer.
// ============================================================================
class ColorLayerUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    int layer;
    std::vector<GLubyte> color;   // la capa entera (4 por corner)
    std::vector<int> index;       // los indices de paleta (vacio si la capa no es byIndex)
public:
    ColorLayerUndo(Mesh* M, int c) : m(M), layer(c) {
        if (!m || layer < 0 || layer >= (int)m->colorLayers.size()) { m = NULL; return; }
        ColorLayer* cl = m->colorLayers[layer];
        if (!cl) { m = NULL; return; }
        color = cl->color;
        index = cl->index;
    }
    bool IsEmpty() const { return !m || color.empty(); }
    void Apply() {
        if (!m || layer < 0 || layer >= (int)m->colorLayers.size()) return;
        ColorLayer* cl = m->colorLayers[layer];
        if (!cl || cl->color.size() != color.size()) return; // la topologia cambio: no tocar
        cl->color.swap(color);
        cl->index.swap(index);
        m->ApplyLayersToRender();   // la capa -> vertexColor[] (lo que dibuja el core)
    }
    W3D_UNDO_NO_INDICES // color/indice: (b) snapshot completo con guard de tamano; m: (a)
};
static ColorLayerUndo* g_pendingColor = NULL;

void UndoColorStart(Mesh* m, int layer) {
    delete g_pendingColor; g_pendingColor = NULL;
    if (m) g_pendingColor = new ColorLayerUndo(m, layer);
}
void UndoColorCancel() { delete g_pendingColor; g_pendingColor = NULL; }
void UndoColorConfirm(bool change) {
    if (change && g_pendingColor && !g_pendingColor->IsEmpty()) { Push(g_pendingColor); g_pendingColor = NULL; }
    UndoColorCancel();
}

void UndoUVStart(Mesh* m) {
    delete g_pendingUV;    g_pendingUV = NULL;
    delete g_pendingUVGeo; g_pendingUVGeo = NULL;
    if (m) g_pendingUV = new UVMapUndo(m);
}
void UndoUVStartComplete(Mesh* m) { // llamar ANTES de UVSeparateFacesSel (snapshot pre-split)
    delete g_pendingUV;    g_pendingUV = NULL;
    delete g_pendingUVGeo; g_pendingUVGeo = NULL;
    if (m) g_pendingUVGeo = new MeshGeoUndo(m);
}
void UndoUVCancel() {
    delete g_pendingUV;    g_pendingUV = NULL;
    delete g_pendingUVGeo; g_pendingUVGeo = NULL;
}
void UndoUVConfirm(bool change) {
    if (change && g_pendingUVGeo)                          { Push(g_pendingUVGeo); g_pendingUVGeo = NULL; }
    else if (change && g_pendingUV && !g_pendingUV->IsEmpty()){ Push(g_pendingUV);    g_pendingUV = NULL; }
    UndoUVCancel(); // descarta lo que haya quedado pendiente (sin cambio real)
}

// ============================================================================
//  ARMATURE 2D del mesh (huesos 2D del editor UV): patron BonesUndo adaptado a W3dBone2D.
//  Snapshot COMPLETO de los huesos del armature 2D ACTIVO (con su hueso activo) + la capa
//  uv y uv2dRest: la POSE 2D
//  deforma mesh->uv (Armature2DApply) y borrar huesos re-aplica el skinning, asi que la
//  capa vuelve byte-exacta con el swap (sin recomputar nada). Los VERTEX GROUPS no se
//  snapshotean (ver nota en Undo.h): Bone2DDelete los conserva a proposito y un grupo de
//  mas tras deshacer un Add es inofensivo.
//  Los TRACKS de los clips 2D SI van: borrar/reordenar huesos remapea Bone2DTrack::bone y
//  se lleva puestas las curvas del hueso borrado -> sin snapshot, el undo devolvia los
//  huesos pero las curvas ya no volvian. Se guardan solo los tracks (el clip -nombre/fps/
//  rango- no lo tocan estas operaciones), en paralelo a los clips de ESE armature.
// ============================================================================
class Bones2DUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    Armature2D* arm;              // el armature 2D CONCRETO que se snapshoteo (no "el activo": el
                                  // usuario puede cambiar de armature entre capturar y deshacer)
    std::vector<W3dBone2D> bones;
    int active;
    std::vector<GLfloat> uv;      // la pose 2D deforma mesh->uv -> viaja con los huesos
    std::vector<GLfloat> rest2d;  // uv2dRest
    // TRACKS de los clips 2D: GEMELO EXACTO del caso de BonesUndo (ver alla). Era un vector
    // paralelo POSICIONAL a arm->anims -familia (2): el "-" de la tarjeta borra un clip 2D sin
    // empujar nada- con el mismo guard de conteo y los mismos dos modos de romper en silencio.
    // Cada entrada lleva SU INDICE y se remapea.
    struct Clip2DTracks { int idx; std::vector<Bone2DTrack> tracks; Clip2DTracks() : idx(-1) {} };
    std::vector<Clip2DTracks> tracks2d;
    bool Vive() const {           // el armature sigue existiendo en la malla?
        if (!m || !arm) return false;
        for (size_t i = 0; i < m->armatures2d.size(); i++) if (m->armatures2d[i] == arm) return true;
        return false;             // lo borraron (Delete del panel): este undo ya no aplica
    }
public:
    Bones2DUndo(Mesh* M) : m(M), arm(NULL), active(-1) {
        if (!m) return;
        arm = m->Arm2DActiveP();
        if (arm) { bones = arm->bones; active = arm->activeBone; }
        if (m->uv && m->vertexSize > 0) uv.assign(m->uv, m->uv + m->vertexSize * 2);
        rest2d = m->uv2dRest;
        if (arm) {
            tracks2d.resize(arm->anims.size());
            for (size_t i = 0; i < arm->anims.size(); i++) {
                tracks2d[i].idx = (int)i;
                if (arm->anims[i]) tracks2d[i].tracks = arm->anims[i]->tracks;
            }
        }
    }
    bool Change() const { // hubo cambio real? (un G+Esc / click sin mover no ensucia el stack)
        if (!Vive()) return false;
        if (arm->activeBone != active) return true;
        if (arm->bones.size() != bones.size()) return true;
        for (size_t i = 0; i < bones.size(); i++) {
            const W3dBone2D& v = arm->bones[i]; const W3dBone2D& s = bones[i];
            if (v.name != s.name || v.parent != s.parent || v.select != s.select) return true;
            if (v.connected != s.connected || v.selHead != s.selHead || v.selTail != s.selTail) return true;
            if (v.headU != s.headU || v.headV != s.headV || v.tailU != s.tailU || v.tailV != s.tailV) return true;
            if (v.poseTU != s.poseTU || v.poseTV != s.poseTV || v.poseRot != s.poseRot ||
                v.poseSX != s.poseSX || v.poseSY != s.poseSY) return true;
        }
        return false;
    }
    void Apply() {
        if (!Vive()) return;
        arm->bones.swap(bones);
        int c = arm->activeBone; arm->activeBone = active; active = c;
        if (arm->activeBone >= (int)arm->bones.size()) arm->activeBone = -1; // guard
        // la capa uv vuelve TAL CUAL estaba (swap byte-exacto, con guard de tamano)
        if (m->uv && (int)uv.size() == m->vertexSize * 2) {
            m->UninstantiateData(W3DMD_UV);   // COW (recurso compartido)
            for (size_t i = 0; i < uv.size(); i++) { GLfloat t = m->uv[i]; m->uv[i] = uv[i]; uv[i] = t; }
        }
        rest2d.swap(m->uv2dRest);
        // tracks de los clips 2D: cada entrada vuelve A SU CLIP (indice remapeado). Crear/borrar
        // clips es OTRA operacion, pero YA NO se salta el swap por conteo: los clips que siguen
        // vivos recuperan lo suyo y los nuevos no reciben nada.
        for (size_t i = 0; i < tracks2d.size(); i++) {
            const int k = tracks2d[i].idx;
            if (k < 0 || k >= (int)arm->anims.size() || !arm->anims[k]) continue;
            arm->anims[k]->tracks.swap(tracks2d[i].tracks);
        }
        m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true;
        m->skinGeomVersion++; // re-subir el VBO de uv
    }
    // (c) tracks2d[].idx: POSICION en Armature2D::anims (clips 2D), familia (2) -> se remapea.
    // (b) bones/uv/rest2d: snapshots completos (por eso los huesos 2D son familia (1)).
    // (c) activo: posicion en arm->bones, vuelve JUNTO con el snapshot completo de los huesos.
    // (a) m: Mesh*; arm: Armature2D* VALIDADO al aplicar (Vive()).
    void RemapList(const W3dRenameDest& list, int x, int y) {
        if (!W3dSameList(W3dDestClip2D(m, arm, -1), list)) return;
        for (size_t i = 0; i < tracks2d.size(); i++)
            if (!RemapIndexSaved(tracks2d[i].idx, x, y)) tracks2d[i].idx = -1; // borraron ESE clip
    }
};
static Bones2DUndo* g_pendingB2D = NULL;
void UndoBones2DCapture(Mesh* m){ if (m) Push(new Bones2DUndo(m)); } // op discreta: push directo (el caller ya valido que va a mutar)
void UndoBones2DStart(Mesh* m){ delete g_pendingB2D; g_pendingB2D = m ? new Bones2DUndo(m) : NULL; }
void UndoBones2DConfirm(){
    if (!g_pendingB2D) return;
    if (!g_pendingB2D->Change()){ delete g_pendingB2D; g_pendingB2D = NULL; return; }
    Push(g_pendingB2D); g_pendingB2D = NULL;
}
void UndoBones2DCancel(){ delete g_pendingB2D; g_pendingB2D = NULL; }

// RENAME de un hueso 2D: comando ATOMICO (ver Undo.h) = snapshot de los huesos (que ya guarda el
// nombre viejo del hueso) + un RenameUndo por cada UV group homonimo. Un solo Ctrl+Z devuelve las
// dos puntas del binding a la vez.
class Bone2DRenameUndo : public ContainerUndo {
    Bones2DUndo* bones;
    std::vector<RenameUndo*> groups;
protected:
    // contenedor: PROPAGA a las dos partes (lo hace ContainerUndo). El 'huesos' tambien tiene
    // que recibirlo: desde que Bones2DUndo lleva los tracks de los clips 2D por indice,
    // saltearlo dejaba ESE snapshot stale (el mismo bug, una capa mas adentro).
    // Aplicar: primero los huesos, despues los nombres de grupo (toggles independientes).
    void Parts(std::vector<UndoCmd*>& out){
        out.push_back(bones);
        for (size_t i=0;i<groups.size();i++) out.push_back(groups[i]);
    }
public:
    Bone2DRenameUndo(Bones2DUndo* h) : bones(h) {}
    ~Bone2DRenameUndo(){ delete bones; for (size_t i=0;i<groups.size();i++) delete groups[i]; }
    void Add(RenameUndo* r){ if (r) groups.push_back(r); }
};
void UndoBone2DRenameCapture(Mesh* m, const std::vector<W3dRenameDest>& groups){
    if (!m) return;
    Bone2DRenameUndo* cmd = new Bone2DRenameUndo(new Bones2DUndo(m));
    // (malla, lista UVGroup, indice): con un std::string* el destino quedaba 'Directo' y el
    // resolver devolvia NULL para siempre -> el undo devolvia el hueso y NO el UV group
    for (size_t i = 0; i < groups.size(); i++)
        if (W3dDestResolve(groups[i])) cmd->Add(new RenameUndo(groups[i]));
    Push(cmd);
}

// ============================================================================
//  ALTA / BAJA de un ARMATURE 2D de la malla (lista Mesh::armatures2d).
//  Guarda el PUNTERO al Armature2D, NO una copia: (1) Armature2D es no copiable (es dueno de
//  sus clips) y (2) los Bones2DUndo pendientes apuntan al puntero CONCRETO y lo validan con
//  Vive() -> si el undo del borrado devolviera OTRO objeto, todos esos pasos anteriores
//  quedaban mudos para siempre. Devolviendo el MISMO puntero, el historial sigue entero.
//  Mientras el armature esta FUERA de la malla el comando es su dueno (lo libera en el dtor);
//  mientras esta DENTRO, la duena es la malla (~Mesh).
//  Los UV GROUPS del rig NO se tocan al borrar (Mesh::Arm2DDelete los deja): son los PESOS por
//  corner y se conservan a proposito, igual que Bone2DDelete conserva los vertex groups. Asi el
//  undo devuelve el rig ya bindeado (el binding es por NOMBRE hueso<->UV group) sin snapshotear
//  nada; un UV group sin hueso es inofensivo (no deforma).
// ============================================================================
class Arm2DListUndo : public UndoCmd {
    // (un objeto que se LIBERA de verdad -el streaming descarga lo generado, una regeneracion- deja de ser de este
    //  paso: Ctrl+Z ya no lo toca, en vez de escribir sobre memoria liberada)
public:
    void UnlinkDetached(Object* deleted) { if ((Object*)m == deleted) m = NULL; }
private:
    Mesh* m;
    Armature2D* arm;   // el armature concreto que se agrego/borro
    int idx;           // su posicion en la lista
    int active;        // Mesh::armature2dActive cuando el armature estaba DENTRO
    bool inside;       // esta AHORA en la malla? (false = lo tenemos nosotros)
    int Index() const {
        if (!m || !arm) return -1;
        for (size_t i = 0; i < m->armatures2d.size(); i++) if (m->armatures2d[i] == arm) return (int)i;
        return -1;
    }
    void Refresh() { // el skinning cambio (un rig entro o salio): re-aplicar y re-subir el VBO
        if (!m) return;
        m->Armature2DRestCapture();
        m->Armature2DApply();
        m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true;
        m->skinGeomVersion++;
        // sacar/devolver un armature REACOMODA el activo (aritmetica sobre el indice) -> el clip
        // que muestra el timeline en kind 4 puede ser OTRO: hay que recargar su rango (sino el
        // Start/End que quedan en pantalla son de un clip ajeno y editarlos lo escribe ahi).
        Arm2DSyncRange(m);
    }
public:
    Arm2DListUndo(Mesh* M, Armature2D* A, int I, int Act, bool Inside)
        : m(M), arm(A), idx(I), active(Act), inside(Inside) {}
    ~Arm2DListUndo() { if (!inside) delete arm; } // quedo afuera de la malla: lo liberamos aca
    void Apply() {
        if (!m || !arm) return;
        if (inside) {                       // SACAR el armature de la malla
            int i = Index(); if (i < 0) return;
            idx = i; active = m->armature2dActive;
            m->armatures2d.erase(m->armatures2d.begin() + i);
            if (m->armature2dActive > i) m->armature2dActive--;
            if (m->armature2dActive >= (int)m->armatures2d.size())
                m->armature2dActive = (int)m->armatures2d.size() - 1;
            DopeRemapIndexKey("arm2d:" + DopeIdOwner(m) + "/a", i, -1); // la seleccion del dope indexa esta lista
            inside = false;
        } else {                            // DEVOLVERLO a su posicion original
            int i = idx;
            if (i < 0) i = 0;
            if (i > (int)m->armatures2d.size()) i = (int)m->armatures2d.size();
            m->armatures2d.insert(m->armatures2d.begin() + i, arm);
            DopeInsertIndexKey("arm2d:" + DopeIdOwner(m) + "/a", i);  // ...y en la vuelta, al reves
            m->armature2dActive = active;
            if (m->armature2dActive < 0 || m->armature2dActive >= (int)m->armatures2d.size())
                m->armature2dActive = i;
            inside = true;
        }
        Refresh();
    }
    // idx/activo son (c): POSICIONES en Mesh::armatures2d. Esa lista es familia (1): sacar o
    // devolver un armature 2D SIEMPRE pasa por UndoArm2DAdded/UndoArm2DDelete, que empujan
    // ESTE mismo comando (nadie la toca "por su cuenta"), y ademas mientras el armature esta
    // DENTRO la posicion se re-resuelve POR PUNTERO (Index()) y al devolverlo se clampea.
    W3D_UNDO_NO_INDICES // arm: (a) Armature2D* (identidad); m: (a) Mesh*
};
// el armature YA se agrego (idx = su posicion): deshacer lo SACA de la lista sin destruirlo.
void UndoArm2DAdded(Mesh* m, int idx) {
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return;
    Push(new Arm2DListUndo(m, m->armatures2d[idx], idx, m->armature2dActive, true));
}
// BORRA el armature idx guardandolo en el paso de undo (no lo destruye: lo adopta el comando).
bool UndoArm2DDelete(Mesh* m, int idx) {
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return false;
    Armature2D* a = m->armatures2d[idx];
    const int actPrev = m->armature2dActive;
    m->armatures2d.erase(m->armatures2d.begin() + idx);
    if (m->armature2dActive > idx) m->armature2dActive--;
    if (m->armature2dActive >= (int)m->armatures2d.size())
        m->armature2dActive = (int)m->armatures2d.size() - 1;
    DopeRemapIndexKey("arm2d:" + DopeIdOwner(m) + "/a", idx, -1); // la seleccion del dope indexa esta lista
    Push(new Arm2DListUndo(m, a, idx, actPrev, false));
    m->Armature2DRestCapture();
    m->Armature2DApply();                 // el rig borrado ya no deforma
    m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true;
    m->skinGeomVersion++;
    Arm2DSyncRange(m);               // el activo cambio: el timeline (kind 4) muestra otro clip
    return true;
}

// ============================================================================
//  STACK DE MODIFICADORES (Mesh::modificadores): AGREGAR / QUITAR / REORDENAR
//
//  Hasta la ronda 11 el stack estaba 100% FUERA del undo: el boton "Remove" de la tarjeta
//  Modifier hacia 'delete modificadores[i]' a secas. Dos consecuencias, y las dos son bugs:
//   1. PERDIDA DE TRABAJO: un modificador con sus parametros (los ejes del Mirror, los
//      niveles de Subdivision, el perfil del Screw) se iba de un click y no habia Ctrl+Z.
//   2. Y la que importa para la familia de fallos de estas rondas: era la UNICA de estas
//      entidades que se LIBERABA por un boton, sin paso de undo, o sea sin ningun invariante
//      de orden que la protegiera. DeleteUndo se guarda un Modifier* (RefEntry::mod) para
//      devolverle su target al deshacer; con el modificador liberado y la direccion reciclada
//      por otro modificador, ese undo escribia el target ENCIMA del modificador de OTRA malla
//      (test 'modtargetuaf'). Ahora el modificador quitado NO se libera: lo adopta este comando,
//      igual que DeleteUndo adopta los objetos borrados y Arm2DListUndo el armature 2D.
//
//  PATRON: SWAP del stack completo (como MeshGeoUndo). Se guarda la lista ENTERA con su ORDEN
//  -no "el que se quito y donde"- asi el mismo comando cubre las tres operaciones y ademas no
//  guarda ninguna POSICION que despues se corra (el "donde" de un Remove se lo comia el
//  reordenar de los botones subir/bajar, que tampoco dejaba paso de undo).
//
//  DUENO DE LA MEMORIA: la malla es la duena de los modificadores que TIENE; este comando es
//  el dueno de los que quedaron en su snapshot y ya NO estan en la malla ('propios'). Esa
//  cuenta se rehace en cada Aplicar (con la malla viva) y NO se puede rehacer despues: cuando
//  ~Object avisa, ~Mesh ya corrio FreeModifiers y la lista de la malla esta vacia (ver
//  el bloque de ganchos en Objects.h). Por eso 'propios' se mantiene EAGER.
// ============================================================================
class ModStackUndo : public UndoCmd {
    Mesh* m;                          // (a) Mesh*; se suelta si lo destruyen (UnlinkDetached)
    std::vector<Modifier*> stack;     // (b) snapshot COMPLETO de Mesh::modificadores, CON su orden
    std::vector<Modifier*> own;       // (a) los del snapshot que ya no estan en la malla: los libera el dtor
    int active;                       // (c)->(b): Mesh::activeModifier, vuelve JUNTO con la lista
    // recalcula que modificadores del snapshot NO estan en la malla (esos son nuestros).
    // Solo vale llamarla con 'm' vivo.
    void RecalculateOwn() {
        own.clear();
        if (!m) return;
        for (size_t i = 0; i < stack.size(); i++) {
            bool inMesh = false;
            for (size_t k = 0; k < m->modifiers.size() && !inMesh; k++)
                if (m->modifiers[k] == stack[i]) inMesh = true;
            if (!inMesh) own.push_back(stack[i]);
        }
    }
public:
    ModStackUndo(Mesh* M) : m(M), stack(M->modifiers), active(M->activeModifier) {}
    // el snapshot se toma ANTES de la operacion; esto se llama DESPUES, con la malla ya
    // cambiada, para saber a quien adoptamos. Va junto en la misma puerta (UndoModAdd y
    // companiia) para que no se pueda olvidar.
    void Sync() { RecalculateOwn(); }
    ~ModStackUndo() { for (size_t i = 0; i < own.size(); i++) delete own[i]; }
    void Apply() {
        if (!m) return;
        std::vector<Modifier*> tmp = m->modifiers;
        m->modifiers = stack; stack = tmp;
        const int act = m->activeModifier; m->activeModifier = active; active = act;
        if (m->activeModifier >= (int)m->modifiers.size())
            m->activeModifier = (int)m->modifiers.size() - 1;   // -1 si quedo vacio
        RecalculateOwn();       // la duena de cada modificador cambio: rehacer la cuenta
        // el stack cambio -> rehacer el preview. Con el stack VACIO tambien hay que llamarla:
        // deja genValid=false y el render vuelve a la malla editable (sino se queda dibujando
        // el resultado del modificador que el Ctrl+Z acaba de sacar).
        m->GenerateMeshModified();
    }
    // la MALLA se esta destruyendo: ~Mesh ya libero los modificadores que ella tenia, pero los
    // NUESTROS no los toco nadie (no estaban en su lista). Se liberan aca y se suelta todo: sin
    // esto quedaban colgados los punteros de 'stack' Y se filtraban los adoptados.
    void UnlinkDetached(Object* deleted) {
        if (deleted != (Object*)m) return;
        for (size_t i = 0; i < own.size(); i++) delete own[i];
        own.clear(); stack.clear(); m = NULL;
    }
    // 'activo' es (c) contra Mesh::modificadores, pero vuelve JUNTO con el snapshot COMPLETO de
    // esa lista (b) -> queda coherente con la lista que restaura, y se clampea. La lista no
    // tiene destinos de rename por indice (no hay W3dRenameDest de modificador) ni pasa por
    // RemapInStacks: si algun dia se puede renombrar un modificador, esto necesita remapeo.
    W3D_UNDO_NO_INDICES
};
// LA PUERTA UNICA del usuario al stack de modificadores (ver Undo.h). Las tres capturan,
// operan y sincronizan la adopcion en el MISMO lugar: no hay forma de hacer la mitad.
static void ModPushSync(ModStackUndo* c) { if (c) { c->Sync(); Push(c); } }
void UndoModAdd(Mesh* m, int type) {
    if (!m) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->AddModifier(type);
    ModPushSync(c);
}
void UndoModRemove(Mesh* m) {
    if (!m || m->activeModifier < 0 || m->activeModifier >= (int)m->modifiers.size()) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->DetachActiveModifier();   // NO lo libera: queda en el snapshot de 'c', que lo adopta
    ModPushSync(c);
}
// VACIA el stack entero sin liberar nada (los adopta el paso de undo). Lo usa el "Apply" de
// Screw/Subdivision, que hornea TODO el stack de una: antes hacia 'delete' en un for y era el
// mismo agujero que el boton Remove (trabajo perdido + direcciones libres para reciclar con un
// DeleteUndo guardandose esos punteros).
void UndoModEmpty(Mesh* m) {
    if (!m || m->modifiers.empty()) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->modifiers.clear(); m->activeModifier = -1;
    ModPushSync(c);
}
void UndoModMove(Mesh* m, int dir) {
    if (!m) return;
    const int i = m->activeModifier, j = i + dir;
    if (i < 0 || i >= (int)m->modifiers.size() || j < 0 || j >= (int)m->modifiers.size()) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->MoveModifier(dir);
    ModPushSync(c);
}

// ============================================================================
//  STACK DE CONSTRAINTS (Object::constraints) - clonado de ModStackUndo
//
//  MISMO patron: se guarda la lista ENTERA con su orden (no "el que se quito y donde"), y el
//  comando adopta los que quedaron afuera de la lista viva. Ver el bloque de ModStackUndo para
//  el por que de cada pieza; aca solo se anotan las DOS diferencias.
//
//  (1) EL ACTIVO VA POR SERIAL, NO POR INDICE. Honestidad primero: con un snapshot de la LISTA
//      ENTERA las dos formas restauran lo MISMO -restaurar la lista capturada devuelve cada
//      constraint a su posicion capturada, asi que el indice y el serial coinciden siempre-, y
//      no hay ningun caso observable que las distinga. La razon de usar el serial no es tapar
//      un bug: es que asi este comando NO TIENE NINGUN MIEMBRO (c). 'activeModifier' guardado
//      crudo ES un indice contra una lista viva, y la tabla de Undo.h tiene que explicar por que
//      se salva; un serial es IDENTIDAD y el W3D_UNDO_NO_INDICES de abajo pasa a ser cierto en
//      vez de una excepcion argumentada. Ademas es la MISMA identidad con la que ya lo nombran
//      la puerta de refs (RefEntry::withSerial) y ConstraintBySerial: una sola forma de decir
//      "este constraint", en vez de dos que hay que mantener de acuerdo.
//
//  (2) LOS DETACHADOS SUELTAN SU FUENTE. Un constraint que ya no esta en el objeto no lo
//      enumera Object::RefsObject, asi que cuando su objeto FUENTE se destruye no hay nadie que
//      le ponga el puntero en NULL: quedaria colgado y la direccion se recicla (la familia
//      'modtargetuaf'). Se limpia en UnlinkDetached, que es justo el aviso de ~Object.
//      OJO: esto es DEFENSA, no algo que un test pueda mirar sin ASAN -- para observarlo habria
//      que liberar el objeto fuente conservando vivo este comando, y lo unico que libera al
//      fuente (que el DeleteUndo se caiga del historial) se lleva puesto tambien a este.
// ============================================================================
class ConstraintStackUndo : public UndoCmd {
    Object* o;                          // (a) Object*; se suelta si lo destruyen (UnlinkDetached)
    std::vector<W3dConstraint*> stack;  // (b) snapshot COMPLETO de Object::constraints, CON su orden
    std::vector<W3dConstraint*> own;    // (a) los del snapshot que ya no estan en el objeto: los libera el dtor
    unsigned int activeSerial;          // IDENTIDAD del elegido en la lista (0 = ninguno). Ver (1) arriba.
    static unsigned int ActiveSerial(const Object* ob) {
        if (!ob || ob->activeConstraint < 0 || ob->activeConstraint >= (int)ob->constraints.size()) return 0;
        return ob->constraints[ob->activeConstraint]->serial.v;
    }
    static int IndexBySerial(const Object* ob, unsigned int s) {
        if (!ob || !s) return -1;
        for (size_t i = 0; i < ob->constraints.size(); i++)
            if (ob->constraints[i]->serial.v == s) return (int)i;
        return -1;   // ya no esta (ej: el Ctrl+Z de un Add): "ninguno elegido", que es la verdad
    }
    void RecalculateOwn() {
        own.clear();
        if (!o) return;
        for (size_t i = 0; i < stack.size(); i++) {
            bool inObject = false;
            for (size_t k = 0; k < o->constraints.size() && !inObject; k++)
                if (o->constraints[k] == stack[i]) inObject = true;
            if (!inObject) own.push_back(stack[i]);
        }
    }
public:
    ConstraintStackUndo(Object* O) : o(O), stack(O->constraints), activeSerial(ActiveSerial(O)) {}
    void Sync() { RecalculateOwn(); }
    ~ConstraintStackUndo() { for (size_t i = 0; i < own.size(); i++) delete own[i]; }
    void Apply() {
        if (!o) return;
        const unsigned int actNow = ActiveSerial(o);   // el estado NUEVO, para el redo
        std::vector<W3dConstraint*> tmp = o->constraints;
        o->constraints = stack; stack = tmp;
        o->SetActiveConstraint(IndexBySerial(o, activeSerial));
        activeSerial = actNow;
        RecalculateOwn();       // el dueno de cada constraint cambio: rehacer la cuenta
        // (no hay nada que regenerar: el stack se evalua AL DIBUJAR, no hornea geometria; el
        //  redibujado lo pide el que llamo al Ctrl+Z, como con cualquier otro paso)
    }
    // el OBJETO se esta destruyendo: ~Object ya libero los constraints que el tenia, pero los
    // NUESTROS no los toco nadie (no estaban en su lista). Y si el que se destruye es un objeto
    // FUENTE, los detachados son los unicos punteros a el que la puerta de refs no enumera.
    void UnlinkDetached(Object* deleted) {
        for (size_t i = 0; i < own.size(); i++)
            if (own[i] && own[i]->sourceObj == deleted) own[i]->sourceObj = NULL;
        if (deleted != o) return;
        for (size_t i = 0; i < own.size(); i++) delete own[i];
        own.clear(); stack.clear(); o = NULL;
    }
    // no guarda NINGUN indice: el activo va por serial (ver arriba) y la lista no tiene destinos
    // de rename por indice (el nombre de un constraint se renombra por PUNTERO, W3dDestName).
    W3D_UNDO_NO_INDICES
};
// ---- las operaciones CRUDAS del stack. A diferencia de las del Mesh viven aca adentro y son
// static a proposito: los constraints no los agrega/saca/mueve ningun otro camino (la carga del
// .w3d hace push_back directo del que leyo), asi que no hay forma de saltearse el paso de undo.
static bool WithNameExists(const std::string& n, void* ctx) {
    const Object* ob = (const Object*)ctx;
    for (size_t i = 0; i < ob->constraints.size(); i++)
        if (ob->constraints[i] && ob->constraints[i]->name == n) return true;
    return false;
}
static void ConstraintPushSync(ConstraintStackUndo* c) { if (c) { c->Sync(); Push(c); } }

void UndoConstraintAdd(Object* o, int type) {
    if (!o) return;
    ConstraintStackUndo* c = new ConstraintStackUndo(o);
    // el nombre arranca en el del TIPO y se uniquifica en el espacio del OBJETO (dos billboards
    // en el mismo objeto -> "Billboard" y "Billboard.001"; en otro objeto vuelve a haber un
    // "Billboard" y no se pisan). Es el mismo espacio que declara W3dNamesCollectSpaces.
    W3dConstraint* nc = new W3dConstraint(type);
    nc->name = W3dUniqueName(W3dConstraintTypeName(type), "Constraint", WithNameExists, o);
    o->constraints.push_back(nc);
    o->activeConstraint = (int)o->constraints.size() - 1;   // el nuevo queda elegido
    ConstraintPushSync(c);
}
void UndoConstraintRemove(Object* o) {
    if (!o || o->activeConstraint < 0 || o->activeConstraint >= (int)o->constraints.size()) return;
    ConstraintStackUndo* c = new ConstraintStackUndo(o);
    // NO se libera: queda en el snapshot de 'c', que lo adopta (si el boton hiciera delete, la
    // direccion quedaria libre para reciclar con un RefEntry todavia guardandose ese serial)
    o->constraints.erase(o->constraints.begin() + o->activeConstraint);
    if (o->activeConstraint >= (int)o->constraints.size())
        o->activeConstraint = (int)o->constraints.size() - 1;   // -1 si quedo vacio
    ConstraintPushSync(c);
}
void UndoConstraintMove(Object* o, int dir) {   // -1 = sube (hacia el principio), +1 = baja
    if (!o) return;
    const int i = o->activeConstraint, j = i + dir;
    if (i < 0 || i >= (int)o->constraints.size() || j < 0 || j >= (int)o->constraints.size()) return;
    ConstraintStackUndo* c = new ConstraintStackUndo(o);
    W3dConstraint* tmp = o->constraints[i]; o->constraints[i] = o->constraints[j]; o->constraints[j] = tmp;
    o->activeConstraint = j;   // el elegido sigue al constraint MOVIDO
    ConstraintPushSync(c);
}

// ============================================================================
//  STACK DE CONSTRAINTS DE LOS HUESOS (W3dBone::constraints)
//
//  NO es un clon de ConstraintStackUndo y a proposito: los constraints de un hueso viven ADENTRO de un
//  W3dBone, que el editor copia POR VALOR (el snapshot de BonesUndo, Shift+D, el borrado, el realloc
//  del vector). Un paso que guardara punteros crudos a esos constraints quedaria colgado en cuanto un
//  Ctrl+Z de huesos reemplaza el vector entero. Por eso el snapshot es POR VALOR tambien: un
//  W3dBoneConstraints por hueso (copia profunda que CONSERVA el serial, ver W3dConstraint.h), y Apply
//  INTERCAMBIA listas con los huesos vivos. Lo que vuelve a la escena es el MISMO constraint (mismo
//  serial): un RefEntry del borrado de su target lo sigue encontrando.
//  Los indices de 'snap' son los de Armature::bones: el historial es LIFO, asi que cuando este paso se
//  aplica los huesos estan como cuando se capturo (el chequeo de tamano es solo defensa).
// ============================================================================
class BoneConstraintsUndo : public UndoCmd {
    Armature* a;                              // (a) se suelta si lo destruyen (UnlinkDetached)
    std::vector<W3dBoneConstraints> snap;     // (b) los stacks de TODOS los huesos, por valor
public:
    BoneConstraintsUndo(Armature* A) : a(A) {
        if (!a) return;
        snap.resize(a->bones.size());
        for (size_t i = 0; i < a->bones.size(); i++) snap[i] = a->bones[i].constraints;
    }
    void Apply() {
        if (!a || a->bones.size() != snap.size()) return;
        for (size_t i = 0; i < snap.size(); i++) a->bones[i].constraints.Swap(snap[i]);
        // la pose se recalcula con el stack de vuelta. poseDirty y NO lastPoseFrame: este ultimo refresca
        // poseT/R/S desde la curva y se comeria la pose a mano (sin keyear) que el usuario esta armando
        a->poseDirty = true;
        g_redraw = true;
    }
    // los del snapshot NO estan en la escena: la puerta de refs no los enumera. Si su target se destruye
    // de verdad hay que soltarlo aca (la familia 'modtargetuaf').
    void UnlinkDetached(Object* deleted) {
        if (deleted == (Object*)a) { a = NULL; snap.clear(); return; }
        for (size_t i = 0; i < snap.size(); i++)
            for (size_t k = 0; k < snap[i].list.size(); k++) {
                W3dConstraint* c = snap[i].list[k];
                if (c && c->sourceObj == deleted) c->sourceObj = NULL;
                if (c && c->poleObj == deleted)   c->poleObj = NULL;
            }
    }
    // 'snap' es PARALELO a Armature::bones, pero los huesos no son una lista con destinos de rename por
    // indice (W3dRenameDest) y se restaura siempre en orden LIFO (ver arriba): nada que remapear.
    W3D_UNDO_NO_INDICES
};
static bool WithBoneConsNameExists(const std::string& n, void* ctx) {
    const W3dBoneConstraints* bc = (const W3dBoneConstraints*)ctx;
    for (size_t i = 0; i < bc->list.size(); i++) if (bc->list[i] && bc->list[i]->name == n) return true;
    return false;
}
void UndoBoneConstraintsCapture(Armature* a) { if (a) Push(new BoneConstraintsUndo(a)); }
void UndoBoneConstraintAdd(Armature* a, int bone, int type) {
    if (!a || bone < 0 || bone >= (int)a->bones.size() || !W3dConstraintTypeForBone(type)) return;
    Push(new BoneConstraintsUndo(a));
    W3dBoneConstraints& bc = a->bones[bone].constraints;
    W3dConstraint* nc = new W3dConstraint(type);
    W3dConstraintBoneDefaults(nc);
    // el nombre, unico en el stack de ESTE hueso (dos IK en huesos distintos se llaman "IK" los dos)
    nc->name = W3dUniqueName(W3dConstraintTypeName(type), "Constraint", WithBoneConsNameExists, &bc);
    // el target por defecto de un constraint de hueso es el PROPIO armature (lo usual en un rig: IK a un
    // hueso controlador, Copy Transforms de DEF a TGT); falta elegir el hueso
    if (W3dConsNeedsTarget(type)) { nc->sourceObj = a; nc->sourceName = a->name; }
    bc.list.push_back(nc);
    bc.active = (int)bc.list.size() - 1;
    a->poseDirty = true;
}
void UndoBoneConstraintRemove(Armature* a, int bone) {
    if (!a || bone < 0 || bone >= (int)a->bones.size()) return;
    W3dBoneConstraints& bc = a->bones[bone].constraints;
    if (bc.active < 0 || bc.active >= (int)bc.list.size()) return;
    Push(new BoneConstraintsUndo(a));   // (el snapshot ya tiene su copia: el de la escena se puede liberar)
    delete bc.list[bc.active];
    bc.list.erase(bc.list.begin() + bc.active);
    if (bc.active >= (int)bc.list.size()) bc.active = (int)bc.list.size() - 1;
    a->poseDirty = true;
}
void UndoBoneConstraintMove(Armature* a, int bone, int dir) {
    if (!a || bone < 0 || bone >= (int)a->bones.size()) return;
    W3dBoneConstraints& bc = a->bones[bone].constraints;
    const int i = bc.active, j = i + dir;
    if (i < 0 || i >= (int)bc.list.size() || j < 0 || j >= (int)bc.list.size()) return;
    Push(new BoneConstraintsUndo(a));
    W3dConstraint* t = bc.list[i]; bc.list[i] = bc.list[j]; bc.list[j] = t;
    bc.active = j;
    a->poseDirty = true;
}

extern void RedoMeshPanelClose();    // RedoMeshPanel.cpp: el panel redo (Add/Normales/Loop Cut) trabaja sobre un snapshot que el undo deja viejo
void (*g_undoChangeHook)() = NULL;   // el panel de la ultima operacion (TransformRedoPanel) se cierra con Ctrl+Z / Ctrl+Y
const void* UndoLast() { return g_undo.empty() ? NULL : (const void*)g_undo.back(); }
void UndoUndo() {
    // el modo mover de la biblioteca a medias: este Ctrl+Z solo lo cancela (ProjectResources.h)
    if (W3dViewRecBeforeUndo()) return;
    if (g_undo.empty()) return;
    RedoMeshPanelClose();
    if (g_undoChangeHook) g_undoChangeHook();
    UndoCmd* c = g_undo.back(); g_undo.pop_back();
    if (!ActivateStepRoot(c)) { delete c; return; }
    c->Apply();            // intercambia: el comando queda con el estado NUEVO
    g_redo.push_back(c);   // disponible para rehacer
    W3dTouchedChanges();   // (lo no guardado: la raiz activa cambio)
    W3dResourcesCheckPurged();   // un material purgado de vuelta en una malla: vuelve con su textura
}
void UndoRedo() {
    if (W3dViewRecBeforeUndo()) return;   // (idem el Ctrl+Y)
    if (g_redo.empty()) return;
    RedoMeshPanelClose();
    if (g_undoChangeHook) g_undoChangeHook();
    UndoCmd* c = g_redo.back(); g_redo.pop_back();
    if (!ActivateStepRoot(c)) { delete c; return; }
    c->Apply();          // intercambia de nuevo: re-aplica el cambio
    g_undo.push_back(c);
    W3dTouchedChanges();
    W3dResourcesCheckPurged();
}
void UndoClear() {
    DeleteCmds(g_undo);   // saca del stack y despues libera (ver DeleteCmds)
    ClearRedo();
    g_groups.clear();     // las bases de los grupos abiertos indexan un stack que ya no existe
    if (g_pendingRep){ delete g_pendingRep; g_pendingRep = NULL; } // mudanza a medio hacer
    if (g_pendingT)  { delete g_pendingT;  g_pendingT  = NULL; }
    if (g_pendingEM) { delete g_pendingEM; g_pendingEM = NULL; }
    if (g_pendingMat){ delete g_pendingMat; g_pendingMat = NULL; }
    // Ningun pendiente puede sobrevivir a un reset de escena. PoseUndo se queda con un Armature* crudo: si la
    // escena se rehace con un transform de pose a medio hacer, ese puntero queda colgado y el proximo
    // UndoPoseConfirm lo desreferencia.
    if (g_pendingKeys){ delete g_pendingKeys; g_pendingKeys = NULL; }
    if (g_pendingPose){ delete g_pendingPose; g_pendingPose = NULL; }
    if (g_pendingWeights){ delete g_pendingWeights; g_pendingWeights = NULL; } // trazo de weight paint a medio hacer
    if (g_pendingBones){ delete g_pendingBones; g_pendingBones = NULL; }       // drag de head/tail a medio hacer
    UndoUVCancel();      // transform de UV a medio hacer (liviano o con split)
    UndoBones2DCancel(); // transform de huesos 2D a medio hacer
}
bool UndoHasSomething() { return !g_undo.empty(); }
bool UndoHasRedo() { return !g_redo.empty(); }

void UndoCaptureMode()                       { Push(new ModeUndo(InteractionMode)); }
// tecla 'h': captura el 'visible' PREVIO ANTES de togglearlo. MISMO gate que ChangeVisibilityObj (Objects.cpp):
// si el Core no va a togglear nada, no ensuciamos el stack con un paso no-op.
void UndoCaptureVisibility() {
    if (InteractionMode == ObjectMode && state == editNavigation && SceneCollection && ActiveObj)
        Push(new VisibilityUndo(ActiveObj));
}
// el OJO y la CAMARITA del outliner sobre una fila (cualquier objeto, no solo el activo; en cualquier modo)
void UndoCaptureVisibilityOf(Object* o) { if (o && SceneCollection) Push(new VisibilityUndo(o)); }
void UndoCaptureRename(const W3dRenameDest& destination) { if (W3dDestResolve(destination)) Push(new RenameUndo(destination)); }
// N nombres en UN SOLO paso de undo: un rename que arrastra su contraparte por
// nombre (vertex group <-> hueso, UV group <-> hueso 2D, refs de scripts, riel de
// camara, targetName...) tiene que volver ENTERO con un Ctrl+Z. En pasos
// separados el estado intermedio deja el binding roto (ya paso con los huesos 2D,
// ver Bone2DRenameUndo). Los NULL y repetidos se descartan.
// FUNDE los ultimos 'n' comandos en UNO solo: Ctrl+Z los deshace juntos. Sirve para
// las operaciones compuestas que se arman con capturas ya existentes (ej: rename de
// hueso 3D = snapshot de bones + los nombres de los vertex groups homonimos). El orden
// de aplicacion se conserva; con n<=1 o el stack corto no hace nada.
static void MergeLast(int n, bool sequential) {
    if (n < 2 || (int)g_undo.size() < n) return;
    CompositeUndo* cmd = new CompositeUndo(sequential);
    for (int i = (int)g_undo.size() - n; i < (int)g_undo.size(); i++) cmd->Add(g_undo[i]);
    g_undo.erase(g_undo.end() - n, g_undo.end());
    g_undo.push_back(cmd);   // NO por Push(): fundir no es una accion nueva (el redo ya se limpio)
}
void UndoMergeLast(int n) { MergeLast(n, false); }

// ---- GRUPO: todo lo empujado entre Iniciar y Fin es UN SOLO Ctrl+Z (ver Undo.h) ----
void UndoGroupStart() { g_groups.push_back(g_undo.size()); }
static void GroupEnd(bool sequential) {
    if (g_groups.empty()) return;
    size_t base = g_groups.back(); g_groups.pop_back();
    if (base > g_undo.size()) base = g_undo.size();   // el desalojo se comio parte del grupo
    const int n = (int)(g_undo.size() - base);
    if (n > 1) MergeLast(n, sequential);
}
void UndoGroupEnd()           { GroupEnd(false); }
void UndoGroupSequentialEnd() { GroupEnd(true); }

// destinos de IDENTIDAD ESTABLE (huesos 3D/2D, mesh parts, refs de lua, capas de la malla):
// ver W3dRenameDest en Undo.h. Los repetidos y los que ya no existen se descartan.
void UndoCaptureRenames(const std::vector<W3dRenameDest>& destinations) {
    if (destinations.empty()) return;
    if (destinations.size() == 1) { UndoCaptureRename(destinations[0]); return; }
    MultiRenameUndo* cmd = new MultiRenameUndo();
    for (size_t i = 0; i < destinations.size(); i++) {
        bool rep = false;
        for (size_t k = 0; k < i; k++) if (SameDest(destinations[k], destinations[i])) { rep = true; break; }
        if (!rep) cmd->Add(destinations[i]);
    }
    if (cmd->IsEmpty()) { delete cmd; return; }
    Push(cmd);
}
void UndoCaptureSelection()                  { Push(new SelectUndo()); }
void UndoCaptureSelectionEdit(Mesh* m)       { if (m) Push(new SelectEditUndo(m)); }
void UndoCaptureMaterial(Mesh* m, int idx)   { if (m) Push(new MaterialUndo(m, idx)); }
void UndoCaptureMeshGeo(Mesh* m)            { if (m) Push(new MeshGeoUndo(m)); } // snapshot ANTES del op

// BORRAR objetos: DETACHA los seleccionados (sin liberar) y guarda el comando. Reemplaza al delete real.
// Devuelve true si detacho algo (el caller no tiene que borrar nada mas).
bool UndoCaptureDeleted(bool incCol) {
    DeleteUndo* d = new DeleteUndo(incCol);
    if (d->IsEmpty()) { delete d; return false; }
    Push(d);
    return true;
}

bool UndoCaptureCreation(bool incCol) {
    DeleteUndo* d = new DeleteUndo(incCol, DeleteUndo::Creation());
    if (d->IsEmpty()) { delete d; return false; }
    Push(d);
    return true;
}

// el paso de undo de un modulo de afuera (ver UndoExternal en Undo.h)
class ExternalUndo : public UndoCmd {
    UndoExternal f;
    void* data;
public:
    ExternalUndo(const UndoExternal& F, void* D) : f(F), data(D) {}
    ~ExternalUndo() { if (f.free) f.free(data); }
    void Apply() { if (f.apply) f.apply(data); }
    // dato: (a)/(d) punteros que el modulo valida al aplicar y valores; ninguna POSICION en una
    // lista (es el contrato de UndoExternal en Undo.h)
    W3D_UNDO_NO_INDICES
    void UnlinkDetached(Object* deleted) { if (f.unlink) f.unlink(data, deleted); }
};
void UndoPushExternal(const UndoExternal& f, void* data) {
    Push(new ExternalUndo(f, data));
}

// ============================================================================
//  REPARENT (ver ReparentUndo arriba): pendiente hasta confirmar.
//
//  DOS DETALLES QUE NO SON COSMETICOS:
//  (1) W3dNamesLoading SUPRIME la captura. Es LA MISMA puerta que ya apaga la
//      renumeracion del embudo (W3dAttachTo) y la levantan los loaders -que arman el arbol a
//      mano y no tienen que dejar historial- y el modal "mover" del outliner, que hace N
//      pasos provisorios y empuja SU PROPIO paso al confirmar (y ninguno al cancelar). El
//      contrato es: el que levanta la puerta es el dueno de empujar el paso.
//  (2) EL PASO DEL ARBOL VA PRIMERO DEL GRUPO, no ultimo. Cuando la mudanza cruza de escena,
//      el embudo empuja ADEMAS el paso de NOMBRES, y ese ya esta en el stack cuando esto
//      confirma. Si el compuesto quedara [nombres, arbol], el Ctrl+Z restauraria el nombre
//      viejo con el objeto TODAVIA en la escena destino -donde ese nombre puede estar
//      tomado-, se re-uniquificaria (W3dFreeNameFor) y el usuario recuperaria "Btn.001"
//      en vez de "Btn". Insertandolo en la BASE del grupo, el Ctrl+Z devuelve primero el
//      arbol y despues los nombres, que es el orden que hace falta (y el redo, al aplicar en
//      el mismo orden, tambien queda bien).
// ============================================================================
void UndoReparentStart(Object* o) {
    // la puerta suprimida se chequea ANTES de descartar el pendiente: adentro de un modal que
    // ya capturo (el "mover" del outliner) los reparents intermedios pasan por aca, y pisarle
    // el snapshot al modal le dejaba el Confirmar sin nada que empujar (la mudanza no quedaba
    // en el historial y el stack de redo NO se vaciaba: justo el agujero del fallo A).
    if (W3dNamesLoading) return;
    if (g_pendingRep) { delete g_pendingRep; g_pendingRep = NULL; }
    if (!o) return;
    g_pendingRep = new ReparentUndo(o);
}
void UndoReparentCancel() { if (g_pendingRep) { delete g_pendingRep; g_pendingRep = NULL; } }
void UndoReparentConfirm() {
    ReparentUndo* c = g_pendingRep;
    g_pendingRep = NULL;
    if (!c) return;
    if (!c->Differs()) { delete c; return; }   // quedo donde estaba: no ensucia el historial
    ClearRedo();                               // accion NUEVA (esto es lo que cierra el fallo A)
    size_t base = g_groups.empty() ? g_undo.size() : g_groups.back();
    if (base > g_undo.size()) base = g_undo.size();
    g_undo.insert(g_undo.begin() + base, c);
    EvictOld();
}

void UndoTransformStart() {
    if (g_pendingT) delete g_pendingT;
    g_pendingT = new TransformUndo();
}
void UndoTransformStartObj(Object* o) {
    if (!o) return;
    if (g_pendingT) delete g_pendingT;
    g_pendingT = new TransformUndo(o);
}
void UndoTransformConfirm() {
    if (!g_pendingT) return;
    // sin cambio real (un click que armo el drag y solto) no se pushea: si no, cada
    // click sobre un elemento del Editor 2D dejaba un paso de undo que "no hacia nada"
    if (!g_pendingT->IsEmpty() && g_pendingT->Differs()) Push(g_pendingT); else delete g_pendingT;
    g_pendingT = NULL;
}
void UndoTransformCancel() {
    if (g_pendingT) { delete g_pendingT; g_pendingT = NULL; }
}

void UndoEditMoveStart(Mesh* m) {
    if (g_pendingEM) delete g_pendingEM;
    g_pendingEM = new EditMoveUndo(m);
}
void UndoEditMoveConfirm() {
    if (!g_pendingEM) return;
    if (!g_pendingEM->IsEmpty()) Push(g_pendingEM); else delete g_pendingEM;
    g_pendingEM = NULL;
}
void UndoEditMoveCancel() {
    if (g_pendingEM) { delete g_pendingEM; g_pendingEM = NULL; }
}

// COLOR (lo llama el ColorPicker al cerrar): pushea solo si el color cambio (cancelar restaura -> no pushea)
void UndoCaptureColor(GLfloat* target, const GLfloat* old) {
    if (!target || !old) return;
    bool change = false; for (int i=0;i<4;i++) if (target[i] != old[i]) change = true;
    if (change) Push(new ColorUndo(target, old));
}

// MODIFICACION de material (checkbox/shininess): pendiente -> snapshot al empezar a tocar, push al soltar.
void UndoMaterialModStart(Material* m) {
    if (!m) return;
    if (g_pendingMat && g_pendingMat->Mat() == m) return; // ya hay snapshot de este material
    if (g_pendingMat) delete g_pendingMat;                // otro material sin commitear -> descarta
    g_pendingMat = new MaterialModUndo(m);
}
void UndoMaterialModCommit() { // lo llama el panel cada frame al soltar el mouse
    if (!g_pendingMat) return;
    if (g_pendingMat->Differs()) Push(g_pendingMat); else delete g_pendingMat;
    g_pendingMat = NULL;
}

// ============================================================================
//  REMAPEO DE LOS COMANDOS PENDIENTES (ver RemapInStacks, arriba)
//  Un pendiente es un comando YA CAPTURADO -con sus indices adentro- que todavia no entro a
//  ningun stack. Se le pasa el mismo aviso que a los stacks; su propio RemapList decide.
//  AL AGREGAR UN PENDIENTE NUEVO, SUMARLO ACA (es la lista completa de los g_pending*).
// ============================================================================
static void RemapInPending(const W3dRenameDest& list, int a, int b) {
    if (g_pendingRep)  g_pendingRep->RemapList(list, a, b);   // ReparentUndo: sin indices de lista
    if (g_pendingT)    g_pendingT->RemapList(list, a, b);
    if (g_pendingEM)   g_pendingEM->RemapList(list, a, b);
    if (g_pendingMat)  g_pendingMat->RemapList(list, a, b);
    if (g_pendingJoin) g_pendingJoin->RemapList(list, a, b);   // MeshGeoUndo: vanimIdx
    if (g_pendingApplyXf) g_pendingApplyXf->RemapList(list, a, b);
    for (size_t i = 0; i < g_pendingApplyGeos.size(); i++) g_pendingApplyGeos[i]->RemapList(list, a, b);
    for (size_t i = 0; i < g_pendingApplyArms.size(); i++) g_pendingApplyArms[i]->RemapList(list, a, b);
    if (g_pendingKeys)  g_pendingKeys->RemapList(list, a, b);  // KeyframesUndo: la lista de curvas
    if (g_pendingBones) g_pendingBones->RemapList(list, a, b); // BonesUndo: tracks[].idx
    if (g_pendingPose)  g_pendingPose->RemapList(list, a, b);
    if (g_pendingWeights) g_pendingWeights->RemapList(list, a, b);
    if (g_pendingUV)    g_pendingUV->RemapList(list, a, b);
    if (g_pendingUVGeo) g_pendingUVGeo->RemapList(list, a, b); // MeshGeoUndo: vanimIdx
    if (g_pendingB2D)   g_pendingB2D->RemapList(list, a, b);   // Bones2DUndo: tracks2d[].idx
}

// ============================================================================
//  PINTURA DE TEXTURA: un trazo = un paso de undo con los pixeles de antes de lo que toco (se intercambian)
// ============================================================================
// (solo lo que el trazo toco, por baldosas: ver TexEditTiles. La textura se busca por su Texture*: su edicion pudo
//  soltarse al salir de pintar -TexEditReleaseIdle- y se vuelve a leer de su archivo, que es lo que tenia)
class PaintedTextureUndo : public UndoCmd {
    Texture* tex;
    TexEditTiles tiles;
public:
    PaintedTextureUndo(TextureEditable* T, bool whole) : tex(T ? T->tex : NULL) { TexEditRecordStart(T, &tiles, whole); }
    ~PaintedTextureUndo() { Stop(); }
    void Stop() { TextureEditable* te = TexEditFind(tex); if (te && te->rec == &tiles) TexEditRecordStop(te); }
    bool IsEmpty() const { return tiles.index.empty(); }
    void Apply() {
        TextureEditable* te = TexEditGet(tex);
        if (!te) return;   // la textura se libero: no hay nada que deshacer
        if (TexEditSwapTiles(te, tiles)) g_redraw = true;
    }
    W3D_UNDO_NO_INDICES // tex: se revalida en TexEditFind/TexEditGet (Textures[]); tiles: propias
};
static PaintedTextureUndo* g_pendingTex = NULL;
void UndoTextureStart(TextureEditable* te, bool whole) {
    delete g_pendingTex; g_pendingTex = NULL;
    if (te) g_pendingTex = new PaintedTextureUndo(te, whole);
}
void UndoTextureConfirm(bool change) {
    if (g_pendingTex) g_pendingTex->Stop();
    if (change && g_pendingTex && !g_pendingTex->IsEmpty()) { Push(g_pendingTex); g_pendingTex = NULL; }
    delete g_pendingTex; g_pendingTex = NULL;
}
