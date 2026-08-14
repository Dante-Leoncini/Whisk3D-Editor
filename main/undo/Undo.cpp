#include "Undo.h"
#include "variables.h"           // InteractionMode, ObjActivo, ObjSelects
#include "objects/Objects.h"     // Object (pos/rot/scale)
#include "objects/Mesh.h"        // Mesh (edit move: vertex/normals/CalcularBordes)
#include "objects/EditMesh.h"    // EditMesh (seleccion de sub-elementos: vertSel/edgeSel/faceSel)
#include "objects/ObjectMode.h" // W3dNombreLibrePara: re-chequear el espacio al restaurar un nombre
#include "W3dNombres.h"          // W3dNombreUnico: el nombre del constraint nuevo, en el espacio del objeto
#include "w3dlog.h"             // w3dLogfW: los re-uniquificados del undo quedan registrados
#include "W3dAviso.h"           // W3dAvisof: la curva huerfana que vuelve en una escena RECREADA se avisa
#include "objects/Light.h"       // Lights (global): el borrado de luces lo des/re-registra
#include "objects/Materials.h"    // Materials (lista global): destino de rename por INDICE (W3dDestGlobal)
#include "edit/Modifier.h"       // Modifier (limpiar target de Armature/Mirror/Boolean al borrar el objeto apuntado)
#include "objects/Armature.h"    // Armature (cast correcto Object*<->Armature* al limpiar/restaurar skinArmature)
#include "script/W3dScript.h"    // W3dScriptEntrada::refs (destino RefLua de un rename: vive en un vector por valor)
#include "animation/SkeletalAnimation.h" // KeyframesUndo: recorrer las curvas del clip activo (tracks/Propertys)
#include "animation/Animation.h"         // AnimationObjects / keyFrame / ActiveAnimKind
#include "animation/Armature2DAnimation.h" // clips del armature 2D (kind 4): undo de sus keyframes
#include "animation/VertexAnimation.h"   // KeyframesUndo: snapshot binario de los frames de la vertex anim de objeto
#include "render/UIOverlay.h"    // UI2D_EsElemento2D / Rot2dDe / TamanoElem (campos 2D del transform)
#include "objects/Texto2D.h"     // el texto 2D escala por su 'tam'
#include "objects/UI.h"          // resize del lienzo (UI::ancho/alto)
// CameraActive: NO incluyo Camera.h (header pesado del editor, arrastra Target/Curve/icons -> riesgo en el
// build de Symbian). Forward-declaro: solo necesito el puntero (Object es la 1ra base -> el cast a Object* es offset 0).
class Camera; extern Camera* CameraActive;
#include <vector>
#include <set>
#include <string>
#include <stdio.h>   // sprintf GLOBAL (prefijos del remapeo del dope); Symbian/STLport no tiene std::snprintf
// SceneCollection (raiz de la escena) viene de objects/Objects.h

extern void ActualizarEditMeshActivo(); // LayoutInput.cpp (refresca g_editMesh al cambiar de modo)
// SELECCION DEL DOPE SHEET (Timeline.cpp): sus claves llevan el INDICE del armature 2D adentro
// ("arm2d:#<serial malla>/a<IDX>/b<n>"), asi que sacar o devolver un armature CORRE esos indices
// igual que corre los del undo. Se declara aca en vez de incluir Timeline.h (header pesado de UI).
extern void DopeRemapIndiceClave(const std::string& prefijo, int a, int b);
extern void DopeInsertarIndiceClave(const std::string& prefijo, int a);
// el DUENO adentro de esas claves es "#<serial>", NO el nombre (el nombre se recicla: ver el
// bloque de identidad de Timeline.h). Los prefijos de los avisos se arman con esto.
extern std::string DopeIdDueno(const Object* o);
// las filas de VERTEX ANIM se resuelven por la anim ACTIVA -> todo camino que la cambie tiene
// que soltar esa seleccion (ver DopeSoltarVertexAnim en Timeline.h)
extern void DopeSoltarVertexAnim();
extern void InvalidarSkinDeArmature(Armature* a); // ObjectMode.cpp (libera el cache de vertex-anim de las mallas del rig)

// ============================================================================
//  Comandos de UNDO/REDO. Cada comando guarda UN snapshot del estado y su
//  Aplicar() INTERCAMBIA el guardado con el vivo (swap). Asi el MISMO Aplicar()
//  sirve para deshacer (vivo=nuevo -> guardado) y para rehacer (vivo=viejo ->
//  guardado de nuevo): es un toggle. Deshacer pasa el comando del stack de undo
//  al de redo; rehacer al reves. Una accion NUEVA (Push) vacia el stack de redo.
// ============================================================================
class UndoCmd {
public:
    virtual ~UndoCmd() {}
    virtual void Aplicar() = 0; // intercambia el estado guardado con el vivo (undo Y redo)
    // Una lista CORRIO SUS INDICES (se reordeno o se borro un elemento): los miembros que
    // guardan una POSICION en esa lista (un destino de rename, un indice suelto, o un vector
    // PARALELO posicional) tienen que seguir al elemento a su lugar nuevo, sino el proximo
    // Ctrl+Z aplica al elemento equivocado: corrupcion SILENCIOSA (ni crash ni aviso).
    // 'lista' identifica LA LISTA (su .i no se mira); b >= 0 = intercambio a<->b;
    // b < 0 = se BORRO el indice 'a'. Ver UndoMoverCapaMalla / UndoListaBorrada.
    //
    // *** ES PURA A PROPOSITO (defensa por construccion) ***
    // Van CINCO tandas de bugs de esta MISMA clase, siempre en un comando que nadie clasifico.
    // Con el default vacio, agregar un comando nuevo que guarda posiciones "compilaba y andaba"
    // hasta que alguien borraba un elemento de abajo. Ahora NO COMPILA hasta que el autor
    // decide una de las dos cosas, mirando la TABLA de clasificacion de miembros de Undo.h:
    //   - tiene miembros de tipo (c) -> escribe un RemapLista de verdad;
    //   - no tiene ninguno           -> pone W3D_UNDO_SIN_INDICES, que ES la declaracion
    //                                   explicita de "todos mis miembros son (a)/(b)/(d)".
    virtual void RemapLista(const W3dRenameDest& lista, int a, int b) = 0;
    // 'borrado' se esta DESTRUYENDO de verdad (free): todo objeto que este comando tenga
    // DETACHADO (fuera del arbol) y lo referencie tiene que soltar el puntero. El recorrido
    // del destructor (DesvincularDelArbol, Objects.cpp) camina SOLO el arbol vivo y no llega
    // aca. Default vacio: la inmensa mayoria de los comandos no es duena de ningun objeto.
    virtual void DesvincularDetachados(Object* borrado) { (void)borrado; }
};
// "revise mis miembros y NINGUNO es una POSICION ni un vector PARALELO a una lista" (tipos
// (a) puntero validado al aplicar / (b) snapshot completo de la lista / (d) valor propio).
// Es una declaracion, no un atajo: si el comando guarda un indice, esto es MENTIRA y vuelve
// el bug silencioso. Poner al lado la clasificacion de cada miembro, como los demas.
#define W3D_UNDO_SIN_INDICES  void RemapLista(const W3dRenameDest&, int, int) {}

// ============================================================================
//  COMANDO CONTENEDOR: los virtuales se reenvian DESDE UN SOLO LUGAR.
//
//  Hay varios comandos que son "N comandos adentro de uno" (CompuestoUndo -el de
//  UndoFundirUltimos-, MultiRenameUndo, JoinUndo, ApplyUndo, Bone2DRenameUndo). Cada uno
//  reescribia el reenvio a mano, comando por comando y virtual por virtual, y el resultado
//  fue el de siempre: Aplicar y RemapLista se reenviaban, pero DesvincularDetachados NO. O
//  sea que meter un comando que ES DUENO de memoria (DeleteUndo, ModStackUndo) adentro de un
//  compuesto lo dejaba SIN el aviso de "esto se esta liberando" -> puntero muerto adentro de
//  un contenedor, que es exactamente la familia de bugs que se viene cerrando. Y no era
//  hipotetico: el Apply Modifier arma un compuesto (UndoFundirUltimos) con un ModStackUndo
//  adentro.
//
//  *** POR QUE ASI Y NO "acordarse de agregar la linea" ***
//  El contenedor NO reimplementa ningun virtual: declara SUS PARTES (Partes()) y esta clase
//  reenvia TODO. Un virtual nuevo en UndoCmd se agrega ACA una sola vez y le llega a los
//  cinco contenedores solos; olvidarse deja de ser posible porque no hay nada que recordar.
//  El unico virtual que un contenedor puede querer redefinir es Aplicar (MultiRenameUndo
//  negocia UN nombre entre todas sus puntas en vez de aplicar una por una).
//
//  Partes() se arma en cada llamada (no se cachea): las partes de un contenedor son fijas
//  desde que se pushea, pero cachear un vector paralelo seria justo otro estado que puede
//  quedar desincronizado.
// ============================================================================
class ContenedorUndo : public UndoCmd {
protected:
    // cada contenedor enumera aca sus partes (en el ORDEN en que hay que aplicarlas)
    virtual void Partes(std::vector<UndoCmd*>& out) = 0;
public:
    void Aplicar() {
        std::vector<UndoCmd*> p; Partes(p);
        for (size_t i = 0; i < p.size(); i++) if (p[i]) p[i]->Aplicar();
    }
    void RemapLista(const W3dRenameDest& L, int a, int b) {
        std::vector<UndoCmd*> p; Partes(p);
        for (size_t i = 0; i < p.size(); i++) if (p[i]) p[i]->RemapLista(L, a, b);
    }
    void DesvincularDetachados(Object* borrado) {
        std::vector<UndoCmd*> p; Partes(p);
        for (size_t i = 0; i < p.size(); i++) if (p[i]) p[i]->DesvincularDetachados(borrado);
    }
};

// cambio de modo Edit/Object
class ModeUndo : public UndoCmd {
    int modo;
public:
    ModeUndo(int m) : modo(m) {}
    void Aplicar() { int cur = InteractionMode; InteractionMode = modo; modo = cur; ActualizarEditMeshActivo(); }
    W3D_UNDO_SIN_INDICES // modo: (d) valor propio (Edit/Object), no indexa ninguna lista
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
static bool ObjetoEnEscena(Object* o) {
    if (!o || !SceneCollection) return false;
    std::vector<Object*> pila; pila.push_back(SceneCollection);
    while (!pila.empty()) {
        Object* n = pila.back(); pila.pop_back();
        if (n == o) return true;
        for (size_t i = 0; i < n->Childrens.size(); i++) pila.push_back(n->Childrens[i]);
    }
    return false;
}
W3dRenameDest W3dDestNombre(std::string* p) {
    // RED DE SEGURIDAD: 'Directo' se resuelve con W3dNombrePunteroVivo, que SOLO reconoce
    // &Object::name y las puntas de vinculo que enumera Object::RefObjetoNombre (targetName,
    // RielName y la fuente de cada constraint). Un puntero a cualquier otro nombre (un
    // UVGroup::nombre, un W3dBone::name...) no matchea NUNCA -> el resolver devuelve NULL
    // siempre y el paso de undo es un no-op MUDO. Eso rompio el binding del rig 2D en silencio.
    // Las APIs de captura ya no aceptan std::string* pelado (rompe en compilacion), asi que
    // esto solo puede pasar si alguien construye el destino a mano: que quede en el log.
    if (p && !W3dNombrePunteroVivo(p))
        w3dLogfW("[undo] W3dDestNombre con un puntero que NO es un nombre de Object: "
                 "el paso de rename va a ser un no-op (usa W3dDestCapaMalla/Hueso3D/... )");
    W3dRenameDest d; d.tipo = W3dRenameDest::Directo; d.ptr = p; return d;
}
W3dRenameDest W3dDestHueso3D(Armature* a, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::Hueso3D; d.dueno = (void*)a; d.i = idx; return d;
}
W3dRenameDest W3dDestHueso2D(Mesh* m, Armature2D* arm, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::Hueso2D; d.dueno = (void*)m; d.dueno2 = (void*)arm; d.i = idx; return d;
}
W3dRenameDest W3dDestMeshPart(Mesh* m, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::MeshPart; d.dueno = (void*)m; d.i = idx; return d;
}
W3dRenameDest W3dDestRefLua(Object* o, int script, int ref) {
    W3dRenameDest d; d.tipo = W3dRenameDest::RefLua; d.dueno = (void*)o; d.i = script; d.j = ref; return d;
}
W3dRenameDest W3dDestCapaMalla(Mesh* m, int capa, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::CapaMalla; d.dueno = (void*)m; d.cual = capa; d.i = idx; return d;
}
W3dRenameDest W3dDestClipArm(Armature* a, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::ClipArm; d.dueno = (void*)a; d.i = idx; return d;
}
W3dRenameDest W3dDestClip2D(Mesh* m, Armature2D* arm, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::Clip2D; d.dueno = (void*)m; d.dueno2 = (void*)arm; d.i = idx; return d;
}
W3dRenameDest W3dDestGlobal(int lista, int idx) {
    W3dRenameDest d; d.tipo = W3dRenameDest::Global; d.cual = lista; d.i = idx; return d;
}
// LOS DOS DESTINOS APUNTAN A LA MISMA LISTA? (el indice NO se mira: identifica al contenedor,
// no al elemento). Es lo que usa el remapeo para saber a que destinos les corrio el indice:
// una lista es (tipo + dueno + dueno2 + cual). Los tipos que no tienen alguno de esos campos
// lo dejan en 0 en TODOS sus destinos, asi que comparar los cuatro siempre alcanza.
static bool W3dMismaLista(const W3dRenameDest& d, const W3dRenameDest& lista) {
    return d.tipo == lista.tipo && d.dueno == lista.dueno &&
           d.dueno2 == lista.dueno2 && d.cual == lista.cual;
}
// remapea UN indice guardado, con la misma convencion que RemapLista: b >= 0 = intercambio
// a<->b; b < 0 = se BORRO el indice 'a'. Devuelve false si el indice guardado apuntaba JUSTO
// al elemento borrado (el que lo guardo decide que hacer: los destinos de rename mueren, los
// pasos de reordenar tambien, un vector paralelo pierde ESA entrada).
// Vive aca arriba porque lo usan comandos de todo el archivo (MeshGeoUndo es el primero).
static bool RemapIndiceGuardado(int& idx, int a, int b) {
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
static bool Arm2DEnMalla(Mesh* m, Armature2D* arm) {
    if (!m || !arm) return false;
    for (size_t k = 0; k < m->armatures2d.size(); k++) if (m->armatures2d[k] == arm) return true;
    return false;
}
std::string* W3dDestResolver(const W3dRenameDest& d) {
    switch (d.tipo) {
        case W3dRenameDest::Directo:
            // Directo = un nombre que es MIEMBRO de un Object (Object::name / targetName /
            // RielName): no se recrea, pero el OBJETO se puede borrar (y liberar al caerse del
            // stack de undo). Se chequea que el puntero siga siendo uno de esos miembros en un
            // objeto de la escena; si no, NULL -> no-op seguro en vez de escribir en un colgado.
            if (!d.ptr || !W3dNombrePunteroVivo(d.ptr)) return NULL;
            return d.ptr;
        case W3dRenameDest::Hueso3D: {
            Armature* a = (Armature*)d.dueno;
            if (!a || !ObjetoEnEscena(a)) return NULL;
            if (d.i < 0 || d.i >= (int)a->bones.size()) return NULL;
            return &a->bones[d.i].name;
        }
        case W3dRenameDest::Hueso2D: {
            Mesh* m = (Mesh*)d.dueno; Armature2D* arm = (Armature2D*)d.dueno2;
            if (!m || !ObjetoEnEscena(m) || !Arm2DEnMalla(m, arm)) return NULL;
            if (d.i < 0 || d.i >= (int)arm->huesos.size()) return NULL;
            return &arm->huesos[d.i].nombre;
        }
        case W3dRenameDest::MeshPart: {
            Mesh* m = (Mesh*)d.dueno;
            if (!m || !ObjetoEnEscena(m)) return NULL;
            if (d.i < 0 || d.i >= (int)m->materialsGroup.size()) return NULL;
            return &m->materialsGroup[d.i].name;
        }
        case W3dRenameDest::RefLua: {
            Object* o = (Object*)d.dueno;
            if (!o || !ObjetoEnEscena(o) || !o->scriptDatos) return NULL;
            if (d.i < 0 || d.i >= (int)o->scriptDatos->scripts.size()) return NULL;
            std::vector<std::pair<std::string, std::string> >& refs = o->scriptDatos->scripts[d.i].refs;
            if (d.j < 0 || d.j >= (int)refs.size()) return NULL;
            return &refs[d.j].second;
        }
        // CAPAS de la malla: vertex groups / UV groups / UV maps / capas de color / armatures 2D /
        // vertex anims. Todas son vector<T*> y el elemento SE LIBERA: Mesh::LiberarCapas (o sea
        // CUALQUIER op de geometria, via MeshGeoUndo::AplicarA) las destruye y las rehace con new,
        // y el boton "-" de las tarjetas borra la activa. Por eso van por (malla, lista, indice).
        case W3dRenameDest::CapaMalla: {
            Mesh* m = (Mesh*)d.dueno;
            if (!m || !ObjetoEnEscena(m)) return NULL;
            switch (d.cual) {
                case W3dRenameDest::VGroup:
                    if (d.i < 0 || d.i >= (int)m->vertexGroups.size() || !m->vertexGroups[d.i]) return NULL;
                    return &m->vertexGroups[d.i]->nombre;
                case W3dRenameDest::UVGroup:
                    if (d.i < 0 || d.i >= (int)m->uvGroups.size() || !m->uvGroups[d.i]) return NULL;
                    return &m->uvGroups[d.i]->nombre;
                case W3dRenameDest::UVMap:
                    if (d.i < 0 || d.i >= (int)m->uvMaps.size() || !m->uvMaps[d.i]) return NULL;
                    return &m->uvMaps[d.i]->nombre;
                case W3dRenameDest::ColorLayer:
                    if (d.i < 0 || d.i >= (int)m->colorLayers.size() || !m->colorLayers[d.i]) return NULL;
                    return &m->colorLayers[d.i]->nombre;
                case W3dRenameDest::Arm2D:
                    if (d.i < 0 || d.i >= (int)m->armatures2d.size() || !m->armatures2d[d.i]) return NULL;
                    return &m->armatures2d[d.i]->nombre;
                case W3dRenameDest::VertAnim:
                    if (d.i < 0 || d.i >= (int)m->animations.size() || !m->animations[d.i]) return NULL;
                    return &m->animations[d.i]->name;
            }
            return NULL;
        }
        case W3dRenameDest::ClipArm: {   // clip 3D: Armature::animations (se borra con el "-" de la tarjeta)
            Armature* a = (Armature*)d.dueno;
            if (!a || !ObjetoEnEscena(a)) return NULL;
            if (d.i < 0 || d.i >= (int)a->animations.size() || !a->animations[d.i]) return NULL;
            return &a->animations[d.i]->name;
        }
        case W3dRenameDest::Clip2D: {    // clip del armature 2D: Armature2D::anims
            Mesh* m = (Mesh*)d.dueno; Armature2D* arm = (Armature2D*)d.dueno2;
            if (!m || !ObjetoEnEscena(m) || !Arm2DEnMalla(m, arm)) return NULL;
            if (d.i < 0 || d.i >= (int)arm->anims.size() || !arm->anims[d.i]) return NULL;
            return &arm->anims[d.i]->name;
        }
        case W3dRenameDest::Global: {    // listas del PROYECTO (no cuelgan de ningun objeto)
            if (d.cual == W3dRenameDest::MaterialG) {
                if (d.i < 0 || d.i >= (int)Materials.size() || !Materials[d.i]) return NULL;
                return &Materials[d.i]->name;
            }
            if (d.cual == W3dRenameDest::SceneAnimG) {
                if (d.i < 0 || d.i >= (int)SceneAnimations.size() || !SceneAnimations[d.i]) return NULL;
                return &SceneAnimations[d.i]->name;
            }
            return NULL;
        }
    }
    return NULL;
}
// mismo destino? (para descartar repetidos al capturar varias puntas de un vinculo)
static bool MismoDest(const W3dRenameDest& a, const W3dRenameDest& b) {
    if (a.tipo != b.tipo) return false;
    if (a.tipo == W3dRenameDest::Directo) return a.ptr == b.ptr;
    return a.dueno == b.dueno && a.dueno2 == b.dueno2 && a.cual == b.cual && a.i == b.i && a.j == b.j;
}

// ============================================================================
//  RENAME (objeto/material/uv/color/hueso/...): el destino es un W3dRenameDest, que
//  se resuelve al APLICAR (arriba). El nombre GUARDADO puede haber sido TOMADO por
//  otro mientras el paso estaba en el stack, asi que al restaurarlo hay que
//  RE-CHEQUEAR el espacio de nombres (fallo F2):
//     "add cube / objname Zzz / add cube / undo"  ->  dos 'Cubo.001' vivos.
//  W3dNombreLibrePara (ObjectMode.cpp) ubica el espacio del puntero por IDENTIDAD y
//  devuelve el siguiente libre; si el puntero NO es un nombre (una ref de lua, un
//  targetName) lo devuelve tal cual: esos son el OTRO extremo de un vinculo y tienen
//  que volver al valor exacto, no uniquificarse.
// ============================================================================
class RenameUndo : public UndoCmd {
    W3dRenameDest dest;
    std::string   guardado;
public:
    // NO hay ctor desde std::string*: el destino SIEMPRE es un W3dRenameDest (ver Undo.h).
    RenameUndo(const W3dRenameDest& D) : dest(D) {
        std::string* p = W3dDestResolver(dest);
        if (p) guardado = *p;
    }
    const std::string& Guardado() const { return guardado; }
    std::string* Destino() const { return W3dDestResolver(dest); } // se resuelve RECIEN AHORA
    void Aplicar() { AplicarCon(NULL); }
    // 'forzar' != NULL: escribe ESE valor (ya negociado por MultiRenameUndo entre todas
    // las puntas del vinculo) en vez de re-uniquificar por su cuenta.
    void AplicarCon(const std::string* forzar) {
        std::string* t = W3dDestResolver(dest);
        if (!t) return;   // el dueno (o el elemento) ya no existe: no-op SEGURO
        std::string cur = *t;
        std::string vuelve = guardado;
        if (forzar) vuelve = *forzar;
        else {
            const char* etq = NULL;
            vuelve = W3dNombreLibrePara(t, guardado, &etq);
            if (vuelve != guardado)
                w3dLogfW("[nombres] undo: '%s' ya estaba tomado (%s) -> vuelve como '%s'",
                         guardado.c_str(), etq ? etq : "?", vuelve.c_str());
        }
        *t = vuelve;
        guardado = cur;
    }
    // la lista corrio sus indices: el destino sigue a SU elemento (ver UndoMoverCapaMalla /
    // UndoListaBorrada). b >= 0 = swap a<->b; b < 0 = se borro el indice 'a' (el destino de
    // ESE elemento muere -no-op seguro- y los de arriba bajan uno).
    void RemapLista(const W3dRenameDest& lista, int a, int b) {
        if (!W3dMismaLista(dest, lista)) return;
        if (b >= 0) {
            if      (dest.i == a) dest.i = b;
            else if (dest.i == b) dest.i = a;
        }
        else if (dest.i == a) dest.i = -1;   // el dueno del nombre ya no existe
        else if (dest.i >  a) dest.i--;
    }
};

// N comandos cualesquiera en UN SOLO paso (ver UndoFundirUltimos en Undo.h)
class CompuestoUndo : public ContenedorUndo {
    std::vector<UndoCmd*> partes;
protected:
    void Partes(std::vector<UndoCmd*>& out){ out = partes; }   // el reenvio de los virtuales lo hace ContenedorUndo
public:
    ~CompuestoUndo(){ for (size_t i = 0; i < partes.size(); i++) delete partes[i]; }
    void Agregar(UndoCmd* c){ if (c) partes.push_back(c); }
};

// ============================================================================
//  RENAME MULTIPLE en UN SOLO paso (ver UndoCapturarRenames en Undo.h): todas las
//  puntas de UN vinculo por nombre (vertex group + hueso 3D, uv group + hueso 2D,
//  objeto + refs de lua + targetName/RielName). Todas guardan EL MISMO nombre viejo.
//  Al restaurar, si ese nombre se lo tomo otro (fallo F2) NO alcanza con uniquificar
//  cada punta por separado: cada espacio daria un sufijo distinto y el vinculo quedaria
//  ROTO (justo lo que este comando existe para evitar). Se negocia UN nombre libre en
//  TODOS los espacios involucrados y se escribe el mismo en todas.
// ============================================================================
class MultiRenameUndo : public ContenedorUndo {
    std::vector<RenameUndo*> partes;
protected:
    void Partes(std::vector<UndoCmd*>& out){ for (size_t i = 0; i < partes.size(); i++) out.push_back(partes[i]); }
public:
    ~MultiRenameUndo(){ for (size_t i = 0; i < partes.size(); i++) delete partes[i]; }
    void Agregar(const W3dRenameDest& d){ if (W3dDestResolver(d)) partes.push_back(new RenameUndo(d)); }
    bool Vacio() const { return partes.empty(); }
    void Aplicar(){
        if (partes.empty()) return;
        const std::string pedido = partes[0]->Guardado();
        // negociacion: se pide 'pedido' a cada espacio; el que lo rechaza propone otro y
        // se vuelve a preguntar a todos. W3dNombreLibrePara siempre avanza el sufijo, asi
        // que converge en pocas vueltas (el tope es una red de seguridad).
        std::string cand = pedido;
        for (int intento = 0; intento < 64; intento++) {
            std::string prop = cand;
            for (size_t i = 0; i < partes.size(); i++) {
                if (partes[i]->Guardado() != pedido) continue;   // no es punta de ESTE nombre
                std::string libre = W3dNombreLibrePara(partes[i]->Destino(), prop, NULL);
                if (libre != prop) prop = libre;
            }
            if (prop == cand) break;
            cand = prop;
        }
        if (cand != pedido)
            w3dLogfW("[nombres] undo: '%s' ya estaba tomado -> las %d punta(s) del vinculo vuelven como '%s'",
                     pedido.c_str(), (int)partes.size(), cand.c_str());
        for (size_t i = 0; i < partes.size(); i++) {
            if (partes[i]->Guardado() == pedido) partes[i]->AplicarCon(&cand);
            else                                 partes[i]->Aplicar();   // otro nombre: por su cuenta
        }
    }
};

// seleccion de objetos: la lista seleccionada + el activo
class SelectUndo : public UndoCmd {
    std::vector<Object*> sel;
    Object* activo;
public:
    SelectUndo() { sel = ObjSelects; activo = ObjActivo; }
    void Aplicar() {
        std::vector<Object*> curSel = ObjSelects; Object* curAct = ObjActivo; // estado vivo
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = false;
        ObjSelects = sel; ObjActivo = activo;
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = true;
        sel = curSel; activo = curAct; // guarda lo que estaba vivo (para rehacer)
    }
    W3D_UNDO_SIN_INDICES // sel/activo: (a) punteros a Object (el arbol de escena no se indexa)
};

// transform en OBJECT MODE: pos/rot/escala de los seleccionados al EMPEZAR
// rotEuler va aparte del quaternion: el quaternion no distingue 0 de 360, asi que sin guardarlo el undo de un
// giro de vuelta entera te devolvia la orientacion pero te comia las vueltas (y con ellas la animacion).
// Los ELEMENTOS 2D guardan ADEMAS sus campos propios (rot2d / ancho / alto o el tam del
// texto, y el ancho/alto del UI): los G/R/S y handles del Editor 2D tocan ESO, no scale.
struct TEst { Object* o; Vector3 pos; Quaternion rot; Vector3 rotEuler; Vector3 scale;
              bool es2d; float rot2d, w2, h2; };
class TransformUndo : public UndoCmd {
    std::vector<TEst> e;
    // que campos 2D tiene 'o' y sus valores actuales (rot2d solo en Elemento2D)
    static bool Leer2D(Object* o, float* r, float* w, float* h) {
        *r = 0.0f; *w = 0.0f; *h = 0.0f;
        if (UI2D_EsElemento2D(o)) {
            *r = *UI2D_Rot2dDe(o);
            float *ew, *eh;
            if (UI2D_TamanoElem(o, &ew, &eh)) { *w = *ew; *h = *eh; }
            else *w = ((Texto2D*)o)->tam;
            return true;
        }
        if (o->getType() == ObjectType::ui) {
            *w = ((UI*)o)->ancho; *h = ((UI*)o)->alto;
            return true;
        }
        return false;
    }
    static void Escribir2D(Object* o, float r, float w, float h) {
        if (UI2D_EsElemento2D(o)) {
            *UI2D_Rot2dDe(o) = r;
            float *ew, *eh;
            if (UI2D_TamanoElem(o, &ew, &eh)) { *ew = w; *eh = h; }
            else ((Texto2D*)o)->tam = w;
        } else if (o->getType() == ObjectType::ui) {
            ((UI*)o)->ancho = w; ((UI*)o)->alto = h;
        }
    }
public:
    TransformUndo() {
        for (size_t i = 0; i < ObjSelects.size(); i++) Capturar(ObjSelects[i]);
    }
    TransformUndo(Object* solo) { Capturar(solo); }   // un objeto puntual (resize del lienzo)
    void Capturar(Object* o) {
        if (!o) return;
        TEst t; t.o = o; t.pos = o->pos; t.rot = o->Rot(); t.rotEuler = o->rotEuler; t.scale = o->scale;
        t.es2d = Leer2D(o, &t.rot2d, &t.w2, &t.h2);
        e.push_back(t);
    }
    bool Vacio() const { return e.empty(); }
    // hubo cambio real? Un drag que no se movio (un click) no merece un paso de undo.
    bool Difiere() const {
        for (size_t i = 0; i < e.size(); i++) {
            Object* o = e[i].o; if (!o) continue;
            if (o->pos.x != e[i].pos.x || o->pos.y != e[i].pos.y || o->pos.z != e[i].pos.z) return true;
            if (o->rotEuler.x != e[i].rotEuler.x || o->rotEuler.y != e[i].rotEuler.y ||
                o->rotEuler.z != e[i].rotEuler.z) return true;
            if (o->scale.x != e[i].scale.x || o->scale.y != e[i].scale.y || o->scale.z != e[i].scale.z) return true;
            if (e[i].es2d) {
                float r, w, h; Leer2D(o, &r, &w, &h);
                if (r != e[i].rot2d || w != e[i].w2 || h != e[i].h2) return true;
            }
        }
        return false;
    }
    void Aplicar() {
        for (size_t i = 0; i < e.size(); i++) {
            Object* o = e[i].o; if (!o) continue;
            Vector3 cp = o->pos; Quaternion cr = o->Rot(); Vector3 ce = o->rotEuler; Vector3 cs = o->scale; // vivo
            o->pos = e[i].pos; o->scale = e[i].scale;
            o->SetRotSnapshot(e[i].rot, e[i].rotEuler);    // tal cual se capturo (con sus vueltas)
            e[i].pos = cp; e[i].rot = cr; e[i].rotEuler = ce; e[i].scale = cs; // guarda lo vivo
            if (e[i].es2d) {
                float r, w, h; Leer2D(o, &r, &w, &h);           // vivo 2D
                Escribir2D(o, e[i].rot2d, e[i].w2, e[i].h2);
                e[i].rot2d = r; e[i].w2 = w; e[i].h2 = h;
            }
        }
    }
    W3D_UNDO_SIN_INDICES // e[]: (a) TEst.o es un Object* + (d) sus pos/rot/escala. No es paralelo
                         // a ninguna lista: cada entrada LLEVA su puntero, no una posicion.
};

// mover verts/aristas/caras en EDIT MODE (move PURO): intercambia las posiciones+normales de la malla.
class EditMoveUndo : public UndoCmd {
    Mesh* m;
    // POSICIONES + ESTADO DE POSE en un solo valor (W3dPosVerts, ver Mesh.h): restaurar
    // posiciones es restaurar tambien EN QUE ESPACIO estan (modelo o pose de un cuadro).
    W3dPosVerts vpos;
    std::vector<GLbyte>  normals;
public:
    EditMoveUndo(Mesh* M) : m(M) {
        if (m) vpos.Capturar(m);
        if (m && m->normals && m->vertexSize > 0) normals.assign(m->normals, m->normals + m->vertexSize * 3);
    }
    bool Vacio() const { return vpos.pos.empty(); }
    void Aplicar() {
        if (!m) return;
        {
            // deshacer/rehacer un move TAMBIEN mueve posiciones: las claves de sharp/seam
            // son los BYTES de la posicion, asi que sin el guard el Ctrl+Z de un move
            // dejaba las marcas ancladas donde ya no hay ningun vertice (antes: se perdian
            // en silencio al guardar; ahora ademas CalcularBordes las podaria). El guard
            // CIERRA ANTES del CalcularBordes de abajo: primero re-anclar, despues podar.
            W3dMoverVerts mv(m);
            vpos.IntercambiarCon(m);   // <- posiciones Y estado de pose, en el mismo swap
            if (m->normals && (int)normals.size() == m->vertexSize * 3) for (size_t i = 0; i < normals.size(); i++) { GLbyte  c = m->normals[i]; m->normals[i] = normals[i]; normals[i] = c; }
        }
        // move PURO = NO cambia la topologia -> CalcularBordes(false) CONSERVA la edit mesh (no la rebuildea)
        // asi NO se pierde la SELECCION; SincronizarPos re-lee las posiciones restauradas al display del edit.
        m->CalcularBordes(false);
        if (m->edit) m->edit->SincronizarPos();
    }
    W3D_UNDO_SIN_INDICES // m: (a) Mesh*; vertex/normals: (b) snapshot completo de los arrays
};

// SELECCION de sub-elementos en EDIT MODE (verts/edges/faces): intercambia los 3 vectores de seleccion
// + el activo de la EditMesh (solo si el size matchea -> robusto al rearmado de la edit).
class SelectEditUndo : public UndoCmd {
    Mesh* m;
    std::vector<unsigned char> vs, es, fs;
    int activo;
public:
    SelectEditUndo(Mesh* M) : m(M), activo(-1) {
        if (m) { m->EnsureEdit();
                 if (m->edit) { vs = m->edit->vertSel; es = m->edit->edgeSel; fs = m->edit->faceSel; activo = m->edit->activeIdx; } }
    }
    void Aplicar() {
        if (!m) return; m->EnsureEdit(); if (!m->edit) return;
        if (m->edit->vertSel.size() == vs.size()) m->edit->vertSel.swap(vs);
        if (m->edit->edgeSel.size() == es.size()) m->edit->edgeSel.swap(es);
        if (m->edit->faceSel.size() == fs.size()) m->edit->faceSel.swap(fs);
        int cur = m->edit->activeIdx; m->edit->activeIdx = activo; activo = cur;
        m->edit->Recolorear(); // re-tinta los buffers segun la seleccion restaurada
    }
    // vs/es/fs: (b) snapshot completo de los 3 vectores de seleccion (con guard de tamano).
    // 'activo' es (c) una posicion en la EDIT MESH, pero esa no es una lista con nombre ni
    // tiene destinos de rename ni botones de reordenar/borrar: se rearma entera de la geo.
    W3D_UNDO_SIN_INDICES
};

// cambiar el MATERIAL de un mesh part (AccionMaterialElegido): intercambia el Material* del mesh part
class MaterialUndo : public UndoCmd {
    Mesh* m; int idx; Material* guardado;
public:
    MaterialUndo(Mesh* M, int i) : m(M), idx(i),
        guardado((M && i >= 0 && i < (int)M->materialsGroup.size()) ? M->materialsGroup[i].material : NULL) {}
    void Aplicar() {
        if (m && idx >= 0 && idx < (int)m->materialsGroup.size()) {
            Material* cur = m->materialsGroup[idx].material;
            m->materialsGroup[idx].material = guardado; guardado = cur;
        }
    }
    // 'idx' es (c): POSICION en Mesh::materialsGroup (la lista de los mesh parts). Hoy esa
    // lista es familia (1) -reordenar/borrar un mesh part empuja UndoCapturarMallaGeo, que la
    // restaura entera, y por LIFO los indices vuelven solos-, asi que NADIE la remapea. Igual
    // se implementa: es gratis y deja el comando correcto el dia que aparezca un borrado de
    // mesh part que no sea deshacible (ahi solo hay que agregar el UndoListaBorrada).
    void RemapLista(const W3dRenameDest& lista, int a, int b) {
        if (!W3dMismaLista(W3dDestMeshPart(m, -1), lista)) return;
        if (!RemapIndiceGuardado(idx, a, b)) idx = -1;   // borraron ESE mesh part: no-op seguro
    }
};

// snapshot COMPLETO de la geometria de la malla (topologia): extrude / delete-edit / loop-cut / duplicate /
// assign mesh part. Clona TODOS los arrays de render + faces3d + materialsGroup + las capas (uv/color/grupos).
// Aplicar() = SWAP: snapshotea la geo viva, escribe la guardada, y se queda con la que estaba viva (redo).
class MeshGeoUndo : public UndoCmd {
    Mesh* m;
    int vertexSize, facesSize;
    // POSICIONES + ESTADO DE POSE en un solo valor (W3dPosVerts, ver Mesh.h). Antes esto era
    // un std::vector<GLfloat> pelado y el flag Mesh::posadaPorAnim NO se snapshoteaba: deshacer
    // una op de topologia hecha en un cuadro POSADO, despues de volver al cuadro base, escribia
    // el vertex[] POSADO con el flag apagado y el CalcularBordes de AplicarA podaba TODAS las
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
    std::vector<UVMap>       uvMaps;       int uvMapActivo;
    std::vector<ColorLayer>  colorLayers;  int colorActivo;
    std::vector<VertexGroup> vertexGroups; int grupoActivo;
    // UV GROUPS (pesos por CORNER): sus indices son RENDER-VERTS, o sea que una op de topologia
    // los re-mapea (GenerarRender) -> viajan con la geometria, como vertCtrlPoint y las anims.
    std::vector<UVGroup>     uvGroups;     int uvGrupoActivo;
    // rest del skinning 2D: es PAREJO a uv[] (invariante uv = f(uv2dRest, pose), ver Mesh.h). Si
    // no viajara con la geo, deshacer una op de topologia dejaba un rest de otro layout.
    std::vector<GLfloat>     uv2dRest;
    std::vector<int>         vertCtrlPoint; int skinNCtrl; // SKINNING: mapeo render-vert -> control-point (sino el undo de una malla skinneada deja el mapeo viejo -> skin roto)
    std::set<std::string>    sharpEdges, seamEdges; // bordes sharp/seam (por POSICION). meshSmooth = shading
    bool                     meshSmooth;
    // FRAMES de las VERTEX ANIMS: el remap de topologia (RemapVertexAnims via GenerarRender)
    // los reescribe al layout nuevo. Sin snapshot, deshacer una op de topologia dejaba la
    // geometria VIEJA con frames del layout NUEVO -> anim muerta (guard de vcount) o, peor,
    // el proximo remap los leia desalineados y la malla animada se rompia. Viajan con la geo.
    VtxAnimSnap              vanims;
    // ---- y ACA vivia el fallo (c) de este comando --------------------------------------
    // vanims.anims es un vector PARALELO POSICIONAL a Mesh::animations, y VertexAnimRestaurarSnap
    // (Core) lo restaura POR POSICION. Pero Mesh::animations es familia (2): el "-" de la tarjeta
    // borra una vertex anim SIN empujar ningun paso de undo -> los indices se corren y este
    // snapshot escribia los frames de cada anim ENCIMA de la de abajo (silencioso: el unico guard
    // era "misma cantidad de keyframes"). NO se puede arreglar snapshoteando la LISTA entera
    // (opcion (i) de Undo.h): eso resucitaria la anim borrada, y el borrado NO es deshacible.
    // Se guarda entonces, para cada slot del snapshot, EL INDICE VIVO al que pertenece, y se
    // remapea como cualquier otro indice (-1 = su anim se borro -> ese slot no se restaura).
    std::vector<int>         vanimIdx;   // vanimIdx[k] = indice en Mesh::animations del slot k
    void CapturarIdxVanims() {
        vanimIdx.clear();
        for (size_t k = 0; k < vanims.anims.size(); k++) vanimIdx.push_back((int)k);
    }
    // escribe el snapshot de frames en la malla RESPETANDO el remapeo (slot k -> anim vanimIdx[k]).
    // Se arma un VtxAnimSnap temporal ya ORDENADO como la lista viva y se reusa el restaurador del
    // Core tal cual (los slots sin snapshot quedan con vcount 0 = "no tocar"). Los buffers se
    // MUEVEN con swap (sin copiar los frames) y se devuelven al terminar.
    void RestaurarVanims(Mesh* s) {
        if (!vanims.tiene) return;
        VtxAnimSnap tmp; tmp.tiene = true;
        tmp.anims.resize(s->animations.size());
        std::vector<int> puestos(vanims.anims.size(), -1);
        for (size_t k = 0; k < vanims.anims.size(); k++) {
            const int d = (k < vanimIdx.size()) ? vanimIdx[k] : (int)k;
            if (d < 0 || d >= (int)tmp.anims.size()) continue;   // su anim se borro (o ya no existe)
            tmp.anims[d].vcount = vanims.anims[k].vcount;
            tmp.anims[d].frames.swap(vanims.anims[k].frames);
            puestos[k] = d;
        }
        VertexAnimRestaurarSnap(s, tmp);
        for (size_t k = 0; k < vanims.anims.size(); k++)          // devolver los buffers prestados
            if (puestos[k] >= 0) vanims.anims[k].frames.swap(tmp.anims[puestos[k]].frames);
    }

    void CapturarDe(Mesh* s) { // llena los miembros desde la malla viva
        vertexSize = s->vertexSize; facesSize = s->facesSize;
        VertexAnimSnapshot(s, vanims); // los frames de las vertex anims, parejos a ESTA geometria
        CapturarIdxVanims();           // recien capturado: cada slot esta en su lugar (identidad)
        sharpEdges = s->sharpEdges; seamEdges = s->seamEdges; meshSmooth = s->meshSmooth;
        normals.clear(); uv.clear(); color.clear(); faces.clear();
        uvMaps.clear(); colorLayers.clear(); vertexGroups.clear(); uvGroups.clear();
        uv2dRest = s->uv2dRest;
        vpos.Capturar(s);   // posiciones Y estado de pose (la unica puerta; ver Mesh.h)
        if (s->normals)     normals.assign(s->normals, s->normals + vertexSize * 3);
        if (s->uv)          uv.assign(s->uv, s->uv + vertexSize * 2);
        if (s->vertexColor) color.assign(s->vertexColor, s->vertexColor + vertexSize * 4);
        if (s->faces)       faces.assign(s->faces, s->faces + facesSize);
        faces3d = s->faces3d; looseEdges = s->looseEdges; looseVerts = s->looseVerts; materialsGroup = s->materialsGroup;
        for (size_t i = 0; i < s->uvMaps.size(); i++)       uvMaps.push_back(*s->uvMaps[i]);
        for (size_t i = 0; i < s->colorLayers.size(); i++)  colorLayers.push_back(*s->colorLayers[i]);
        for (size_t i = 0; i < s->vertexGroups.size(); i++) vertexGroups.push_back(*s->vertexGroups[i]);
        for (size_t i = 0; i < s->uvGroups.size(); i++)     uvGroups.push_back(*s->uvGroups[i]);
        uvMapActivo = s->uvMapActivo; colorActivo = s->colorActivo; grupoActivo = s->grupoActivo;
        uvGrupoActivo = s->uvGrupoActivo;
        vertCtrlPoint = s->vertCtrlPoint; skinNCtrl = s->skinNCtrl; // skinning: mapeo render->control-point
    }
    void AplicarA(Mesh* s) { // escribe los miembros (snapshot) a la malla viva
        delete[] s->normals;     s->normals = NULL;
        delete[] s->uv;          s->uv = NULL;
        delete[] s->vertexColor; s->vertexColor = NULL;
        delete[] s->faces;       s->faces = NULL;
        s->vertexSize = vertexSize; s->facesSize = facesSize;
        vpos.EscribirEn(s);   // vertex[] Y posadaPorAnim: el CalcularBordes del final poda con el gate CORRECTO
        if (!normals.empty()) { s->normals = new GLbyte[normals.size()];       for (size_t i=0;i<normals.size();i++) s->normals[i]     = normals[i]; }
        if (!uv.empty())      { s->uv = new GLfloat[uv.size()];                for (size_t i=0;i<uv.size();i++)      s->uv[i]          = uv[i]; }
        if (!color.empty())   { s->vertexColor = new GLubyte[color.size()];    for (size_t i=0;i<color.size();i++)   s->vertexColor[i] = color[i]; }
        if (!faces.empty())   { s->faces = new MeshIndex[faces.size()];         for (size_t i=0;i<faces.size();i++)   s->faces[i]       = faces[i]; }
        s->faces3d = faces3d; s->looseEdges = looseEdges; s->looseVerts = looseVerts; s->materialsGroup = materialsGroup;
        s->sharpEdges = sharpEdges; s->seamEdges = seamEdges; s->meshSmooth = meshSmooth;
        s->LiberarCapas();
        for (size_t i = 0; i < uvMaps.size(); i++)       s->uvMaps.push_back(new UVMap(uvMaps[i]));
        for (size_t i = 0; i < colorLayers.size(); i++)  s->colorLayers.push_back(new ColorLayer(colorLayers[i]));
        for (size_t i = 0; i < vertexGroups.size(); i++) s->vertexGroups.push_back(new VertexGroup(vertexGroups[i]));
        for (size_t i = 0; i < uvGroups.size(); i++)     s->uvGroups.push_back(new UVGroup(uvGroups[i]));
        s->uvMapActivo = uvMapActivo; s->colorActivo = colorActivo; s->grupoActivo = grupoActivo;
        s->uvGrupoActivo = uvGrupoActivo; s->uv2dRest = uv2dRest;
        s->vertCtrlPoint = vertCtrlPoint; s->skinNCtrl = skinNCtrl; // restaurar el mapeo render->control-point (skinning)
        RestaurarVanims(s); // los frames de las vertex anims vuelven JUNTO con su geometria (mismo layout), CADA UNO A SU ANIM
        s->lastSkinFrame = -999999; // forzar re-skin con la geo/mapeo restaurados (CalcularBordes ya bumpea skinGeomVersion)
        s->CalcularBordes(); // recomputa edges/centroGeom + invalida el edit (se rearma de la geo restaurada)
    }
public:
    MeshGeoUndo(Mesh* M) : m(M), vertexSize(0), facesSize(0), uvMapActivo(0), colorActivo(0), grupoActivo(0),
                           uvGrupoActivo(-1), skinNCtrl(0) {
        if (m) CapturarDe(m);
    }
    void Aplicar() {
        if (!m) return;
        MeshGeoUndo cur(m);   // snapshotea la geo VIVA (la nueva)
        AplicarA(m);          // escribe la geo GUARDADA (la vieja) a la malla
        // queda con lo que estaba vivo (para rehacer): intercambia los buffers con cur
        vertexSize = cur.vertexSize; facesSize = cur.facesSize;
        vpos.Intercambiar(cur.vpos); // posiciones + estado de pose (para rehacer), juntos
        normals.swap(cur.normals); uv.swap(cur.uv); color.swap(cur.color);
        faces.swap(cur.faces); faces3d.swap(cur.faces3d); looseEdges.swap(cur.looseEdges); looseVerts.swap(cur.looseVerts);
        materialsGroup.swap(cur.materialsGroup);
        uvMaps.swap(cur.uvMaps); colorLayers.swap(cur.colorLayers); vertexGroups.swap(cur.vertexGroups);
        uvGroups.swap(cur.uvGroups); uv2dRest.swap(cur.uv2dRest);
        vertCtrlPoint.swap(cur.vertCtrlPoint); skinNCtrl = cur.skinNCtrl; // (faltaban: el redo escribia el mapeo VIEJO -> skin roto)
        uvMapActivo = cur.uvMapActivo; colorActivo = cur.colorActivo; grupoActivo = cur.grupoActivo;
        uvGrupoActivo = cur.uvGrupoActivo;
        sharpEdges.swap(cur.sharpEdges); seamEdges.swap(cur.seamEdges); meshSmooth = cur.meshSmooth;
        vanims.anims.swap(cur.vanims.anims); vanims.tiene = cur.vanims.tiene; // los frames de las anims (para rehacer)
        vanimIdx.swap(cur.vanimIdx);  // el snapshot nuevo salio de la lista VIVA -> sus indices son la identidad
        // la geometria cambio -> refrescar el preview del modificador (subdivision/screw). Antes esto lo hacia el
        // regen POR FRAME de ActualizarEditMeshActivo; ahora que ese esta gateado, hay que pedirlo aca explicito.
        if (!m->modificadores.empty()) m->GenerarMallaModificada();
    }
    // (c) vanimIdx / vanims: vector PARALELO a Mesh::animations (vertex anims), familia (2)
    //     -> se remapea aca (ver el bloque de arriba).
    // (b) el resto (arrays de render, faces3d, materialsGroup, uvMaps/colorLayers/vertexGroups/
    //     uvGroups, uv2dRest, vertCtrlPoint...) es snapshot COMPLETO: AplicarA reemplaza las
    //     listas enteras, asi que sus indices internos (uvMapActivo, grupoActivo...) vuelven
    //     coherentes con la lista que restauran. Por eso esas cuatro capas son familia (1).
    // (a) m: Mesh* (los callers lo validan; el comando muere con la escena).
    void RemapLista(const W3dRenameDest& lista, int a, int b) {
        if (!W3dMismaLista(W3dDestCapaMalla(m, W3dRenameDest::VertAnim, -1), lista)) return;
        for (size_t k = 0; k < vanimIdx.size(); k++)
            if (!RemapIndiceGuardado(vanimIdx[k], a, b)) vanimIdx[k] = -1; // borraron ESA anim
    }
};

// COLOR (RGBA) de un material/luz: intercambia los 4 floats del target. Lo usa el ColorPicker al cerrar.
// el material sigue VIVO? Los materiales del proyecto son globales y hoy NADIE los borra (Materials nunca
// hace erase y ~Material esta vacio), pero el chequeo es de una linea y es lo que le pide el criterio de
// Undo.h a un miembro (a): "puntero validado al aplicar". El material POR DEFECTO no entra a la lista
// (ver el ctor en Materials.cpp) y tambien es un destino legitimo del panel.
static bool MaterialVivo(const Material* m) {
    if (!m) return false;
    if (m == MaterialDefecto) return true;
    for (size_t i = 0; i < Materials.size(); i++) if (Materials[i] == m) return true;
    return false;
}
class ColorUndo : public UndoCmd {
    GLfloat* t; GLfloat val[4];
public:
    ColorUndo(GLfloat* target, const GLfloat* viejo) : t(target) { for (int i=0;i<4;i++) val[i]=viejo[i]; }
    void Aplicar() { if (!t) return; for (int i=0;i<4;i++) { GLfloat c=t[i]; t[i]=val[i]; val[i]=c; } }
    // t: (a) puntero al RGBA de un MATERIAL o de una LUZ; val[4]: (d) valores propios.
    // No se puede revalidar un GLfloat* pelado (no dice de quien es), asi que POR QUE ES SEGURO:
    //  - materiales: no se borran nunca (ver MaterialVivo);
    //  - luces: son Objects y SI se borran, pero el unico que los LIBERA es el destructor de un
    //    DeleteUndo con enEscena==false, y ese comando esta SIEMPRE MAS ARRIBA en g_undo que este
    //    (se pusheo despues). El desalojo por MAXU saca el del FRENTE, o sea el MAS VIEJO: este
    //    ColorUndo se destruye ANTES que el DeleteUndo que libera su luz. Y por LIFO, para llegar a
    //    aplicarlo hay que deshacer primero el borrado, que devuelve la luz a la escena.
    //  Si algun dia se puede borrar un material, o si un comando pasa a liberar objetos sin ser el
    //  tope del stack, esto necesita un destino tipado (como W3dRenameDest) en vez de un float*.
    W3D_UNDO_SIN_INDICES
};

// MODIFICACION de un material (checkboxes + shininess; los COLORES van por ColorUndo). Snapshot SOLO de
// esos campos (NO name/texture/capas -> esos tienen su propio undo y no hay que revertirlos de mas).
class MaterialModUndo : public UndoCmd {
    // cuantos flags copian Leer/Escribir (textureOn..chrome). Los buffers Y los loops usan ESTA
    // constante: antes el miembro era bool[10] y los loops iban hasta 11 -> el de Aplicar() ESCRIBIA
    // b[10], o sea FUERA del arreglo, pisando el miembro de al lado (rmode), y Difiere() comparaba
    // el bool basura de cb[10] (daba "cambio" un material que no cambio). GCC lo cantaba con
    // -Waggressive-loop-optimizations. Si se agrega un flag: sumarlo en Leer/Escribir y aca.
    static const int NFLAGS = 11;   // [10] = lineas (aristas por material)
    Material* mat;
    bool b[NFLAGS]; int rmode; float shin; // rmode = reflectMode (era el bool chromeEquirect, ahora int de 3 modos)
    float glinea;                          // grosorLinea (px): viaja con el flag lineas
    static void Leer(Material* s, bool* bo, int& rm, float& sh, float& gl) {
        bo[0]=s->textureOn; bo[1]=s->filtrado; bo[2]=s->transparent; bo[3]=s->vertexColor;
        bo[4]=s->lighting; bo[5]=s->repeat; bo[6]=s->uv8bit; bo[7]=s->culling;
        bo[8]=s->depth_test; bo[9]=s->chrome; bo[10]=s->lineas;
        rm=s->reflectMode; sh=s->shininess; gl=s->grosorLinea;
    }
    static void Escribir(Material* s, const bool* bo, int rm, float sh, float gl) {
        s->textureOn=bo[0]; s->filtrado=bo[1]; s->transparent=bo[2]; s->vertexColor=bo[3];
        s->lighting=bo[4]; s->repeat=bo[5]; s->uv8bit=bo[6]; s->culling=bo[7];
        s->depth_test=bo[8]; s->chrome=bo[9]; s->lineas=bo[10];
        s->reflectMode=rm; s->shininess=sh; s->grosorLinea=gl;
    }
public:
    MaterialModUndo(Material* M) : mat(M), rmode(0), shin(0), glinea(1.0f) { if (mat) Leer(mat, b, rmode, shin, glinea); }
    Material* Mat() const { return mat; }
    bool Difiere() const {
        if (!MaterialVivo(mat)) return false;
        bool cb[NFLAGS]; int crm; GLfloat cs, cg; Leer(mat, cb, crm, cs, cg);
        for (int i=0;i<NFLAGS;i++) if (cb[i]!=b[i]) return true;
        return crm != rmode || cs != shin || cg != glinea;
    }
    void Aplicar() {
        if (!MaterialVivo(mat)) return;   // (a): el puntero se REVALIDA al aplicar, no se usa a ciegas
        bool cb[NFLAGS]; int crm; GLfloat cs, cg; Leer(mat, cb, crm, cs, cg);   // estado vivo
        Escribir(mat, b, rmode, shin, glinea);                                  // restaura el guardado
        for (int i=0;i<NFLAGS;i++) b[i]=cb[i]; rmode=crm; shin=cs; glinea=cg;   // guarda lo vivo (para rehacer)
    }
    // b[NFLAGS] es un arreglo FIJO de flags del material, NO un vector paralelo a una lista.
    W3D_UNDO_SIN_INDICES // mat: (a) Material* REVALIDADO al aplicar (MaterialVivo); b/rmode/shin/glinea: (d) valores propios
};

// ============================================================================
//  BORRAR objetos (Ctrl+Z): los objetos NO se liberan al borrar -> se DETACHAN de la escena (se sacan de su
//  padre) y los GUARDA el comando, que los re-inserta al deshacer. El comando es DUENO mientras estan
//  detachados; si se cae del stack (destructor con enEscena=false) recien ahi los libera de verdad. Maneja
//  las LUCES (global Lights: las saca/agrega para que una luz borrada NO siga iluminando) y la CAMARA ACTIVA.
// ============================================================================
static void DetacharLuces(Object* o) { // saca del global Lights todas las luces del subarbol (dejan de iluminar)
    if (o->getType() == ObjectType::light)
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { Lights.erase(Lights.begin()+i); break; }
    for (size_t i = 0; i < o->Childrens.size(); i++) DetacharLuces(o->Childrens[i]);
}
static void ReattacharLuces(Object* o) { // re-registra las luces del subarbol (sin duplicar)
    if (o->getType() == ObjectType::light) {
        Light* l = (Light*)o; bool ya = false;
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == l) { ya = true; break; }
        if (!ya) Lights.push_back(l);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) ReattacharLuces(o->Childrens[i]);
}
static bool ContieneCamActiva(Object* o) {
    if (CameraActive && (Object*)CameraActive == o) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (ContieneCamActiva(o->Childrens[i])) return true;
    return false;
}
// el ESQUELETO cuyo clip esta activo (ActiveAnimArm/kind): si se borra, la seleccion de animacion queda colgada
// (el timeline seguia mostrando el clip borrado). Al detacharlo se vuelve a ESCENA (kind 0). El destructor del
// Armature tambien lo hace, pero el borrado DETACHA (no destruye, por el undo) -> hay que resetearlo aca.
extern int ActiveAnimKind; extern Armature* ActiveAnimArm; extern Mesh* ActiveAnimMesh;
static bool ContieneAnimActiva(Object* o) {
    if (ActiveAnimArm && (Object*)ActiveAnimArm == o) return true;
    if (ActiveAnimMesh && (Object*)ActiveAnimMesh == o) return true;   // vertex anim (kind 3): la malla en edicion
    for (size_t i = 0; i < o->Childrens.size(); i++) if (ContieneAnimActiva(o->Childrens[i])) return true;
    return false;
}

// 'mio' = el comando SACO de verdad este objeto del arbol y por lo tanto es su DUENO. NO es
// decorativo: es lo unico que separa "lo tengo yo, detachado" de "sigue vivo en la escena", y
// el destructor libera SOLO lo primero. Ver el paso 0 de Detachar.
struct DelEntry { Object* obj; Object* parent; int index; bool mio;
                  DelEntry() : obj(0), parent(0), index(0), mio(false) {} };
// recolecta los "delete-roots": cada objeto SELECCIONADO y borrable (no-collection salvo incCol) cuyo
// ancestro NO es tambien un delete-root. El subarbol se va con su root (no se recursea adentro).
static void RecolectarBorrar(Object* node, bool incCol, std::vector<DelEntry>& out) {
    for (int i = (int)node->Childrens.size()-1; i >= 0; i--) { // alto->bajo: los indices quedan validos al sacar
        Object* c = node->Childrens[i];
        bool borrable = c->select && (incCol || c->getType() != ObjectType::collection);
        if (borrable) { DelEntry e; e.obj = c; e.parent = node; e.index = i; out.push_back(e); }
        else          RecolectarBorrar(c, incCol, out);
    }
}

// hijo que se REPARENTA al abuelo cuando se borra su padre (asi no desaparece de la escena; queda como hijo del
// abuelo preservando su posicion global, v1: solo traslacion, igual que W3dReparent).
// 'movida' = ESTE comando fue el que la movio (y por lo tanto es el que tiene que devolverla).
// Sin esa marca, el replay reponia a ciegas y podia meter el MISMO Object* en DOS Childrens
// (ver el bloque "EL HIJO PRESERVADO PUDO MUDARSE" en Detachar/Aplicar).
struct RepEntry { Object* child; Object* abuelo; Object* borrado; Vector3 posBajoAbuelo; Vector3 posBajoBorrado;
                  bool movida; RepEntry() : child(0), abuelo(0), borrado(0), movida(false) {} };

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
//   - slot >= 0 y conSerial == 0: refs PROPIAS de una clase, enumeradas por el Core
//     (Object::RefsPropias/RefPropia, ver Objects.h): toda la familia Target -Instance, Mirror
//     objeto, Camera y ObjetoScript- mas el RIEL de la camara. Estas SI tienen un vinculo
//     por NOMBRE al lado
//     (targetName / RielName) y hay que llevarselo junto con el puntero, sino el .w3d guardado
//     queda con "target": "<objeto que no esta en el archivo>" (corrupcion muda: al reabrir el
//     vinculo se pierde sin un solo aviso). Es la MISMA conducta que ya tenia el modificador,
//     que al perder su target se guarda sin campo "target".
//   - slot >= 0 y conSerial != 0: la fuente de un CONSTRAINT del objeto. La puerta les da un slot
//     a cada uno DESPUES de las refs propias de la clase, pero esa lista SE REORDENA Y SE BORRA
//     (Add/Remove/Move Up/Down), asi que el slot no alcanza para volver a encontrar la entrada:
//     se guarda ademas el SERIAL y se resuelve por ahi, igual que mod/modSerial.
// ============================================================================
struct RefEntry {
    Object* dueno;      // el que referencia (la Mesh si slot < 0)
    Modifier* mod;      // solo slot < 0
    unsigned int modSerial; // identidad ESTABLE de ese modificador (ver ModVivoEn abajo)
    unsigned int conSerial; // != 0: la ref es la fuente del constraint con ESE serial (el slot se ignora)
    int slot;           // -1 = ref de malla; >= 0 = indice de Object::RefObjeto
    Object* target;     // a quien apuntaba
    std::string nombre; // solo slot >= 0: el vinculo por nombre que acompania al puntero
};
// ============================================================================
//  EL MODIFICADOR SIGUE VIVO Y ES EL MISMO? (no-op seguro de RefEscribir)
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
static bool ModVivoEn(Mesh* m, Modifier* mod, unsigned int serial){
    if (!m || !mod) return false;
    for (size_t i = 0; i < m->modificadores.size(); i++)
        if (m->modificadores[i] == mod) return m->modificadores[i]->serial.v == serial;
    return false;
}
static bool EnSubarbol(Object* raiz, Object* buscado){
    if (!raiz) return false;
    if (raiz == buscado) return true;
    for (size_t i = 0; i < raiz->Childrens.size(); i++) if (EnSubarbol(raiz->Childrens[i], buscado)) return true;
    return false;
}
static bool ApuntaABorrado(Object* target, const std::vector<DelEntry>& ents){
    if (!target) return false;
    for (size_t i = 0; i < ents.size(); i++) if (EnSubarbol(ents[i].obj, target)) return true;
    return false;
}
// recorre la escena y junta TODA referencia por puntero que apunte a un objeto que se borra:
// las de la malla (modificador / skinArmature) y las que el Core enumera solo (familia Target
// + riel de camara). Nadie se acuerda de sumar su clase aca: la enumeracion generica hace que
// una clase nueva con un Object* entre sola.
static void RecolectarRefs(Object* node, const std::vector<DelEntry>& ents, std::vector<RefEntry>& out){
    if (!node) return;
    if (node->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)node;
        for (size_t i = 0; i < m->modificadores.size(); i++){ Modifier* md = m->modificadores[i];
            if (md && ApuntaABorrado(md->target, ents)){
                RefEntry r; r.dueno=m; r.mod=md; r.modSerial=md->serial.v; r.conSerial=0; r.slot=-1; r.target=md->target; out.push_back(r); } }
        if (ApuntaABorrado((Object*)m->skinArmature, ents)){
            RefEntry r; r.dueno=m; r.mod=NULL; r.modSerial=0; r.conSerial=0; r.slot=-1; r.target=(Object*)m->skinArmature; out.push_back(r); }
    }
    const int propias = node->RefsPropias();   // donde empieza el bloque de constraints en la puerta
    const int nref = node->RefsObjeto();
    for (int i = 0; i < nref; i++){
        Object* t = node->RefObjeto(i);
        if (!ApuntaABorrado(t, ents)) continue;
        RefEntry r; r.dueno=node; r.mod=NULL; r.modSerial=0; r.conSerial=0; r.slot=i; r.target=t;
        // del bloque de constraints para arriba, el slot es una POSICION en una lista que se reordena
        // y se borra: la identidad de la entrada es el serial (ver RefEscribir).
        if (i >= propias){
            const size_t k = (size_t)(i - propias);
            if (k < node->constraints.size() && node->constraints[k]) r.conSerial = node->constraints[k]->serial.v;
        }
        std::string* nom = node->RefObjetoNombre(i); if (nom) r.nombre = *nom;
        out.push_back(r);
    }
    for (size_t i = 0; i < node->Childrens.size(); i++) RecolectarRefs(node->Childrens[i], ents, out);
}
// escribe una ref: restaurar=false la SUELTA (detachar/redo), restaurar=true devuelve lo guardado (undo)
static void RefEscribir(RefEntry& r, bool restaurar){
    if (r.slot < 0){
        Mesh* m = (Mesh*)r.dueno;
        // el modificador puede haberse ido (y su direccion reciclada) entre la captura y esto:
        // si no es EL MISMO, no se escribe nada (ver ModVivoEn arriba, test 'modtargetuaf').
        if (r.mod) { if (ModVivoEn(m, r.mod, r.modSerial)) r.mod->target = restaurar ? r.target : NULL; }
        else if (m) m->skinArmature = restaurar ? (Armature*)r.target : NULL;
        if (m) m->lastSkinFrame = -999999;
        return;
    }
    if (!r.dueno) return;
    if (r.conSerial){
        // la fuente de un CONSTRAINT: se busca por SERIAL y se IGNORA el slot. Entre la captura y
        // esto el usuario pudo agregar, borrar o mover entradas del stack, y entonces el slot apunta
        // a OTRO constraint: escribirlo ahi es la misma corrupcion muda que el 'mod' reciclado. Si el
        // constraint ya no existe no se escribe en ningun lado (patron ModVivoEn, test 'modtargetuaf').
        W3dConstraint* c = r.dueno->ConstraintPorSerial(r.conSerial);
        if (!c) return;
        c->fuenteObj    = restaurar ? r.target : NULL;
        c->fuenteNombre = restaurar ? r.nombre : std::string();
        return;
    }
    r.dueno->SetRefObjeto(r.slot, restaurar ? r.target : NULL);
    std::string* nom = r.dueno->RefObjetoNombre(r.slot);
    if (nom) *nom = restaurar ? r.nombre : std::string();
}

// re-uniquifica el nombre de 'o' y de todo su subarbol en el scope donde acaba de
// quedar. PRE-ORDEN (el orden de FindObjectByName). Solo escribe si hizo falta.
static void ReuniquificarSubarbol(Object* o) {
    if (!o) return;
    const std::string viejo = o->name;
    o->SetNameObj(viejo);
    if (o->name != viejo)
        w3dLogfW("[nombres] undo del borrado: '%s' ya estaba tomado -> vuelve como '%s'",
                 viejo.c_str(), o->name.c_str());
    for (size_t i = 0; i < o->Childrens.size(); i++) ReuniquificarSubarbol(o->Childrens[i]);
}

// ============================================================================
//  LA ANIM ACTIVA CAMBIO -> SOLTAR LA SELECCION DE LAS FILAS DE VERTEX ANIM
//
//  La anim activa NO es un solo global: es el PAR (ActiveAnimKind, ActiveAnimMesh) mas el
//  currentAnim de esa malla. Las filas "objanim:/objvtx:/objuv:/objnrm:" del dope se resuelven
//  por la ACTIVA al usarlas, asi que TODO camino que la cambie tiene que soltar esa seleccion
//  (Timeline.h). Estaban cubiertos AnimSelVertex y BorrarVertexAnimDe (Properties.cpp), pero la
//  mitad de la MALLA se escapo: DeleteUndo la toca por los DOS lados -Detachar() la pone en
//  NULL cuando lo borrado contiene la anim activa, y Aplicar() la RESTAURA al deshacer- y no
//  soltaba nada. Repro (test 'dopevtxundo'): anim de MallaZA activa, borrar A, elegir la de
//  MallaZB, Ctrl+Z -> ActiveAnimMesh vuelve a A con la seleccion de B puesta.
//  Se compara ANTES/DESPUES en vez de soltar siempre: soltar de gratis borraria la seleccion
//  del usuario en el caso comun (borrar un objeto que no tiene nada que ver con la anim activa).
static void SoltarDopeSiCambioAnimVertex(int kindPrev, Mesh* meshPrev) {
    if (ActiveAnimKind != kindPrev || ActiveAnimMesh != meshPrev) DopeSoltarVertexAnim();
}

// ============================================================================
//  LIBERAR UN SUBARBOL DETACHADO: EL SUBARBOL ENTERO, NO SOLO LA RAIZ
//
//  El comando de borrado se lleva RAICES (DelEntry), y adentro de cada raiz viaja TODO su
//  subarbol: los descendientes que TAMBIEN estaban seleccionados (RecolectarBorrar corta la
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
//       que resuelve por nombre (FindObjectByName, objeto("...") de lua, targetName,
//       RielName, hueso<->vertex group) cae en el primero en pre-orden.
//   (B) con Parent == NULL (todo objeto de primer nivel nace asi: Object::Object solo mete el
//       puntero en SceneCollection->Childrens, no se pone de Parent) no hay a quien promover:
//       el subarbol queda inalcanzable y NO se libera nunca. Fuga.
//
//  Por eso libera con W3dLiberarSubarbol (Objects.h), que es el MISMO camino que usa el cierre
//  de proyecto: post-orden, sacandole la lista de hijos a cada dueno antes de destruirlo.
//  'hechos' es lo unico propio de aca: la red contra el doble free. Hoy las raices son
//  disjuntas por construccion (RecolectarBorrar corta la recursion en el primer borrable y no
//  anida), pero eso es una propiedad del recolector de hoy y un doble delete no avisa.
// ============================================================================
static void LiberarSubarbolDetachado(Object* raiz, std::set<Object*>& hechos) {
    if (!raiz || hechos.count(raiz) > 0) return;
    std::vector<Object*> pila; pila.push_back(raiz);   // anotar el subarbol ANTES de liberarlo
    while (!pila.empty()) {
        Object* o = pila.back(); pila.pop_back();
        hechos.insert(o);
        for (size_t i = 0; i < o->Childrens.size(); i++) pila.push_back(o->Childrens[i]);
    }
    W3dLiberarSubarbol(raiz);
}

class DeleteUndo : public UndoCmd {
    std::vector<DelEntry> ents;
    std::vector<RepEntry> reps;    // hijos de los borrados, reparentados al abuelo
    std::vector<RefEntry> refs;    // refs de mallas (modificador/skinArmature) a los objetos borrados -> limpiar/restaurar
    bool repsListos;               // las reps (y refs) se computan UNA vez (en el 1er detach)
    std::vector<Object*>  selPrev; Object* actPrev; Camera* camPrev; Object* colPrev; // CollectionActive previa
    int animKindPrev; Armature* animArmPrev; Mesh* animMeshPrev; // seleccion de animacion previa (para restaurar al deshacer)
    bool enEscena; // true = los objetos estan en la escena; false = los tiene este comando (detachados)
    bool liberando; // true mientras el destructor libera lo detachado (ver DesvincularDetachados)
    // La ANIMACION de los borrados se la lleva ESTE comando, igual que los objetos. Tiene que ser asi: las listas de
    // curvas referencian al objeto POR PUNTERO, y cuando el comando muere hace delete del objeto -> el puntero queda
    // colgando. El proximo objeto que se cree puede caer EN ESA MISMA DIRECCION y "heredar" la animacion del muerto
    // (paso: animar un cubo, borrarlo, crear otro cubo y encontrarlo ya animado). Al deshacer vuelven con su objeto.
    //
    // *** POR ESCENA, NO "la lista que este activa" (fallos [C] y [D] de la ronda 7) ***
    // Las curvas de objeto NO viven en una sola lista: hay UNA POR ANIMACION DE ESCENA, y las de la escena ACTIVA
    // estan en el global AnimationObjects (SetEscenaActiva / NuevaEscena / BorrarEscenaActiva swapean la lista
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
    // escena hace delete (Animation.cpp BorrarEscenaActiva) y crear otra hace new (NuevaEscena), asi que el
    // allocator RECICLA el bloque y el puntero muerto vuelve a "validar" contra una escena NUEVA que nunca tuvo
    // esa curva. Reproducido: escenas [S0,S1] con la curva en S1, borrar el objeto, borrar S1, crear una escena
    // -misma direccion- y Ctrl+Z -> la curva caia en la escena nueva (test 'delescena').
    // Ahora es el INDICE en SceneAnimations, remapeado en RemapLista igual que KeyframesUndo hace con KFLista::idx
    // (el notificador ya existia: UndoBorrarEscenaActiva -> UndoListaBorrada -> RemapEnStacks). idx = -1 = su
    // escena murio; ver DevolverAnimaciones para que se hace con la curva huerfana.
    struct AnimGuardada {
        int esc;                 // (c) INDICE en SceneAnimations, remapeado por RemapLista (-1 = escena borrada)
        std::string escNombre;   // (d) el nombre que tenia esa escena: solo para RESUCITARLA si murio
        int pos; AnimationObject a;
        AnimGuardada() : esc(-1), pos(0) {}
    };
    std::vector<AnimGuardada> anims;
    // la lista VIVA de curvas de la escena 'idx', o NULL si ese indice ya no existe. Las de la escena ACTIVA viven
    // en el global AnimationObjects; las de las demas, adentro de su SceneAnimation (son la misma lista, swapeada).
    static std::vector<AnimationObject>* CurvasDeEscena(int idx) {
        InitSceneAnimations();
        if (idx < 0 || idx >= (int)SceneAnimations.size() || !SceneAnimations[idx]) return NULL;
        return (idx == SceneAnimActiva) ? &AnimationObjects : &SceneAnimations[idx]->objetos;
    }
    void QuitarHijo(Object* p, Object* c) {
        for (size_t k = 0; k < p->Childrens.size(); k++)
            if (p->Childrens[k] == c) { p->Childrens.erase(p->Childrens.begin()+k); break; }
    }
    // Se lleva la animacion de CADA objeto que este comando borra (y de todo su subarbol, que se va con el) de
    // TODAS las escenas, no solo de la activa. Vuelve en Reatachar.
    void LlevarseAnimaciones() {
        anims.clear();
        // UNA sola pasada por lista (antes era una por objeto borrado, y cada erase
        // corria los indices de la siguiente -> las posiciones guardadas no habrian servido).
        std::set<Object*> mios;
        for (size_t i = 0; i < ents.size(); i++) {
            if (!ents[i].mio) continue;   // esa raiz sigue en la escena: su animacion no es de este comando
            std::vector<Object*> pila; pila.push_back(ents[i].obj);
            while (!pila.empty()) {
                Object* o = pila.back(); pila.pop_back();
                for (size_t k = 0; k < o->Childrens.size(); k++) pila.push_back(o->Childrens[k]);
                mios.insert(o);
            }
        }
        InitSceneAnimations();
        for (size_t s = 0; s < SceneAnimations.size(); s++) {
            SceneAnimation* esc = SceneAnimations[s]; if (!esc) continue;
            std::vector<AnimationObject>& lst = ((int)s == SceneAnimActiva) ? AnimationObjects : esc->objetos;
            int sacadas = 0;                              // cuantas saque YA de ESTA lista
            for (size_t a = 0; a < lst.size(); ) {
                if (mios.count(lst[a].obj) > 0) {
                    AnimGuardada g; g.esc = (int)s; g.escNombre = esc->name; g.pos = (int)a + sacadas; g.a = lst[a];
                    anims.push_back(g);                   // 'pos' = la posicion ORIGINAL en SU lista
                    lst.erase(lst.begin() + a);
                    sacadas++;
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
    int ResucitarEscena(const std::string& nombre, std::vector<std::pair<std::string,int> >& hechas) {
        for (size_t i = 0; i < hechas.size(); i++) if (hechas[i].first == nombre) return hechas[i].second;
        InitSceneAnimations();
        const std::string libre = SceneAnimNombreLibre(nombre.empty() ? "Scene" : nombre, -1);
        SceneAnimations.push_back(new SceneAnimation(libre));
        const int idx = (int)SceneAnimations.size() - 1;
        hechas.push_back(std::make_pair(nombre, idx));
        w3dLogfW("[undo] curva huerfana: la escena '%s' ya no existe -> recreada como '%s'",
                 nombre.c_str(), libre.c_str());
        W3dAvisof(false, "Animacion recuperada en una escena nueva: '%s'", W3dNombreCorto(libre).c_str());
        return idx;
    }
    void DevolverAnimaciones() {
        // van en orden creciente de pos DENTRO de cada escena (se recolectaron escena por escena),
        // asi cada una entra en su hueco: la anterior ya lo dejo listo.
        std::vector<std::pair<std::string,int> > resucitadas;   // nombre de la escena muerta -> indice recreado
        for (size_t i = 0; i < anims.size(); i++) {
            if (anims[i].esc < 0) anims[i].esc = ResucitarEscena(anims[i].escNombre, resucitadas);
            std::vector<AnimationObject>* lst = CurvasDeEscena(anims[i].esc);
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
    //  DESHACIBLE (ObjectMode.cpp, W3dAdjuntarA) o sea que no vacia el stack de redo, asi
    //  que el usuario puede mover el hijo a OTRO padre en el medio y el camino queda abierto
    //  por los dos lados. Como QuitarHijo(p,c) solo saca 'c' de p->Childrens, sacarlo del
    //  padre EQUIVOCADO era un no-op y el push_back metia el MISMO Object* en una SEGUNDA
    //  lista de hijos: el outliner lo listaba dos veces, el .w3d lo guardaba duplicado y
    //  -cuando el comando se caia del stack- W3dLiberarSubarbol liberaba al hijo aliaseado
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
    //  sacaba de ningun lado... y aun asi ponia enEscena=false: el comando pasaba a creerse
    //  dueno de un objeto que seguia VIVO y colgado en la escena (aliasing -> .w3d con el
    //  nombre repetido -> SEGV al desalojarse el historial). Desde esta ronda reparentar SI
    //  es deshacible y vacia el redo, o sea que la ventana esta cerrada por arriba; esto es
    //  el piso, para cualquier camino que mueva un objeto sin dejar paso de undo.
    //  DOS reglas: (1) si no esta donde el comando lo dejo, se resuelve el padre REAL
    //  (Object::Parent) y se ACTUALIZA e.parent/e.index; (2) si no esta en NINGUNA lista de
    //  hijos, la entrada NO se toca y NO se declara propia (e.mio queda en false): nunca
    //  "dueno" de algo que no se saco de verdad.
    // ========================================================================
    static int IndiceDe(Object* cont, Object* o) {
        if (!cont) return -1;
        for (size_t k = 0; k < cont->Childrens.size(); k++) if (cont->Childrens[k] == o) return (int)k;
        return -1;
    }
    void ResolverPadres() {
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
            e.mio = false;
            if (!e.obj) continue;
            int k = IndiceDe(e.parent, e.obj);
            if (k < 0) {
                Object* real = e.obj->Parent ? e.obj->Parent : SceneCollection;
                k = IndiceDe(real, e.obj);
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
            e.mio = true;   // el paso 3 hace el erase de verdad
        }
    }
    void ComputarReps() {
        reps.clear();
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
                if (!e.mio) continue;   // esa raiz no se va a detachar: sus hijos no se mueven
                // borrar un UI se lleva TODA su interfaz: sus elementos 2D son PARTE del UI
                // (no tiene sentido dejarlos huerfanos en la escena). El subarbol entero viaja
                // detachado con el comando y vuelve completo al deshacer.
                if (e.obj->getType() == ObjectType::ui) continue;
                // BASE (ver Objects.h): 'posBajoAbuelo' se ESCRIBE en ch->pos y va al .w3d. Con la
                // efectiva, borrar un padre con billboard le horneaba al hijo la orientacion de la
                // ultima camara adentro de su posicion, en silencio y sin vuelta atras.
                Vector3 gAbuelo = e.parent->GetGlobalPositionBase();
                // recorre el SUBARBOL del borrado: los descendientes NO seleccionados se PRESERVAN (reparent al abuelo);
                // los SELECCIONADOS se borran con el (se baja a sus hijos). Antes reparentaba TODO hijo directo -> los
                // hijos seleccionados (ej. las mallas de un armature al "borrar todo") sobrevivian en vez de borrarse.
                std::vector<Object*> pila; for (size_t k = 0; k < e.obj->Childrens.size(); k++) pila.push_back(e.obj->Childrens[k]);
                for (size_t k = 0; k < pila.size(); k++) {
                    Object* ch = pila[k];
                    bool esRoot = false;
                    for (size_t j = 0; j < ents.size(); j++) if (ents[j].obj == ch) { esRoot = true; break; }
                    if (esRoot) continue; // ya es su propio delete-root (no deberia caer dentro de otro subarbol)
                    if (ch->select) { // seleccionado -> se borra con el subarbol; seguir bajando a sus hijos
                        for (size_t j = 0; j < ch->Childrens.size(); j++) pila.push_back(ch->Childrens[j]);
                        continue;
                    }
                    // NO seleccionado -> preservar: reparent al abuelo (no se recursea adentro; queda como estaba)
                    RepEntry r; r.child = ch; r.abuelo = e.parent; r.borrado = ch->Parent ? ch->Parent : e.obj;
                    r.posBajoBorrado = ch->pos;
                    r.posBajoAbuelo  = ch->GetGlobalPositionBase() - gAbuelo; // preserva posicion global (BASE)
                    reps.push_back(r);
                }
        }
    }

    void Detachar() {
        // 0) DONDE ESTA HOY CADA RAIZ (ver ResolverPadres): puede haberse mudado desde que este
        //    comando la dejo. Va ANTES de todo lo demas porque decide QUE raices son de este
        //    comando, y de eso dependen las replicas de los hijos y las animaciones.
        ResolverPadres();
        // 1) el reparentado de los hijos de cada objeto borrado hacia el ABUELO. Un hijo que
        //    TAMBIEN se borra (esta en ents) se va con su root -> no se reparenta. Se RECOMPUTA
        //    en cada pasada (tambien en el redo): ver el bloque de arriba.
        ComputarReps();
        if (!repsListos) {
            // refs por PUNTERO al subarbol borrado: de mallas (modificador Armature/Mirror/Boolean o
            // skinArmature) y de objetos (familia Target + riel de camara, ver RefEntry arriba). Estas
            // SI se recolectan una sola vez: el que referencia ya solto puntero Y nombre y no hay accion
            // deshacible que los pueda volver a escribir sin vaciar el stack de redo.
            if (SceneCollection) RecolectarRefs(SceneCollection, ents, refs);
            repsListos = true;
        }
        // 2) mover los hijos reparentados al abuelo (ANTES de detachar el borrado, asi sus luces no se detachan).
        //    Solo si el hijo SIGUE bajo el borrado: si el usuario lo mudo (reparentar no es deshacible), no se
        //    toca -y no se aliasea el puntero en dos listas de hijos-.
        for (size_t i = 0; i < reps.size(); i++) { RepEntry& r = reps[i];
            r.movida = false;
            if (!r.child || !r.abuelo || !r.borrado) continue;
            if (r.child->Parent != r.borrado) continue;
            QuitarHijo(r.borrado, r.child);
            r.child->Parent = r.abuelo;
            r.child->pos = r.posBajoAbuelo;
            r.abuelo->Childrens.push_back(r.child);
            r.movida = true;
        }
        // 3) detachar cada objeto borrado (ya sin esos hijos)
        const int kindAntes = ActiveAnimKind; Mesh* const meshAntes = ActiveAnimMesh; // ver SoltarDopeSiCambioAnimVertex
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
            // el paso 0 ya resolvio padre e indice; si la raiz no aparecio en NINGUNA lista de
            // hijos, e.mio es false y la entrada se saltea ENTERA (tampoco se apagan sus luces:
            // el objeto sigue a la vista y quedaria oscuro sin motivo)
            if (!e.mio) continue;
            // el INDICE se re-resuelve POR PUNTERO justo antes del erase (es la propiedad que
            // la tabla de Undo.h le adjudica a DelEntry::index): el paso 2 y los erase de las
            // otras raices del mismo padre pueden haber corrido las posiciones.
            const int k = IndiceDe(e.parent, e.obj);
            if (k < 0) { e.mio = false; continue; }
            e.index = k;
            e.parent->Childrens.erase(e.parent->Childrens.begin() + k);
            DetacharLuces(e.obj);
            if (ContieneCamActiva(e.obj)) CameraActive = NULL;
            if (ContieneAnimActiva(e.obj)) { ActiveAnimArm = NULL; ActiveAnimMesh = NULL; ActiveAnimKind = 0; } // anim activa (clip o vertex) borrada -> a ESCENA
        }
        // 4) limpiar las refs colgantes: el modificador vuelve a target=NULL ("none") y skinArmature a NULL -> la malla
        //    deja de deformar (vuelve a bind) y no queda apuntando al esqueleto borrado (que este comando liberara).
        //    Lo mismo la familia Target (instancia/espejo/camara/gamepad), la fuente de cada constraint
        //    y el riel: sueltan el puntero
        //    -asi la instancia deja de DIBUJAR al borrado- y tambien el nombre, para que el .w3d no quede guardado
        //    apuntando a un objeto que ya no esta en el archivo.
        for (size_t i = 0; i < refs.size(); i++) RefEscribir(refs[i], false);
        // 5) si CollectionActive quedo FUERA de la escena (se borro), reapuntarla a una coleccion valida (otra bajo la
        //    escena, o la propia SceneCollection). Sin esto, Add Collection / Import parentan a memoria liberada
        //    (no aparecen / crash). El undo la restaura (colPrev).
        if (SceneCollection && (!CollectionActive || !EnSubarbol(SceneCollection, CollectionActive))) {
            Object* c = NULL;
            for (size_t i = 0; i < SceneCollection->Childrens.size() && !c; i++)
                if (SceneCollection->Childrens[i]->getType() == ObjectType::collection) c = SceneCollection->Childrens[i];
            CollectionActive = c ? c : SceneCollection;
        }
        LlevarseAnimaciones();   // la animacion se va CON el objeto: si no, su entrada queda apuntando a un muerto
        PurgarSeleccion();       // ni ObjActivo ni ObjSelects pueden quedar apuntando a lo detachado
        SoltarDopeSiCambioAnimVertex(kindAntes, meshAntes); // la anim activa se fue con lo borrado
        enEscena = false;
    }
    // esta 'o' dentro de alguno de los subarboles que este comando se lleva?
    bool EnLoBorrado(Object* o) const {
        if (!o) return false;
        for (size_t i = 0; i < ents.size(); i++)
            if (ents[i].mio && EnSubarbol(ents[i].obj, o)) return true;   // solo lo que se llevo de verdad
        return false;
    }
    // El BORRADO REAL (Eliminar, ObjectMode.cpp) deja ObjActivo=NULL y ObjSelects vacio. El
    // Detachar() del comando NO lo hacia, asi que el REDO (Ctrl+Y) dejaba los globales apuntando
    // a objetos DETACHADOS de los que este comando es DUENO: cuando el comando se cae del tope
    // del stack (MAXU) su destructor los libera y ObjActivo/ObjSelects quedan COLGADOS -> SEGV en
    // la proxima accion que los use ("add cube / selobj Cubo / delete / undo / redo" + 110 pasos).
    // Se sacan SOLO los que se van con el borrado (no se vacia la seleccion entera): asi el redo
    // de un JOIN -que es geo + DeleteUndo de los mergeados- conserva activa la malla que queda.
    void PurgarSeleccion() {
        for (size_t i = 0; i < ObjSelects.size(); ) {
            if (EnLoBorrado(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + i);
            else i++;
        }
        if (EnLoBorrado(ObjActivo)) ObjActivo = NULL;
    }
public:
    DeleteUndo(bool incCol) : repsListos(false), actPrev(NULL), camPrev(NULL), colPrev(NULL), enEscena(true), liberando(false) {
        selPrev = ObjSelects; actPrev = ObjActivo; camPrev = CameraActive; colPrev = CollectionActive;
        animKindPrev = ActiveAnimKind; animArmPrev = ActiveAnimArm; animMeshPrev = ActiveAnimMesh;
        if (SceneCollection) RecolectarBorrar(SceneCollection, incCol, ents);
        Detachar(); // el borrado YA paso: los saca de la escena (sin liberar)
    }
    bool Vacio() const { return ents.empty(); }
    void Aplicar() {
        if (enEscena) { Detachar(); return; } // redo del borrado
        // undo: re-inserta cada root en su padre (bajo->alto: los indices guardados quedan validos).
        // SOLO las que este comando saco de verdad (e.mio): una raiz que no se pudo detachar sigue
        // colgada en la escena y re-insertarla la dejaria en DOS listas de hijos.
        for (int i = (int)ents.size()-1; i >= 0; i--) { DelEntry& e = ents[i];
            if (!e.mio) continue;
            int idx = e.index; if (idx < 0) idx = 0; if (idx > (int)e.parent->Childrens.size()) idx = (int)e.parent->Childrens.size();
            e.parent->Childrens.insert(e.parent->Childrens.begin()+idx, e.obj);
            ReattacharLuces(e.obj);
            e.mio = false;   // vuelve a ser de la escena
        }
        // devolver los hijos reparentados a su objeto original (restaurando su posicion local).
        // SOLO los que movio ESTE comando y que siguen donde los dejo: si el usuario los mudo
        // despues (reparentar no es deshacible, no vacia el redo), reponerlos no es lo que pidio
        // y ademas dejaria el MISMO Object* en dos listas de hijos -> doble free / SEGV.
        for (size_t i = 0; i < reps.size(); i++) { RepEntry& r = reps[i];
            if (!r.movida) continue;
            r.movida = false;
            if (!r.child || !r.abuelo || !r.borrado) continue;
            if (r.child->Parent != r.abuelo) continue;   // el usuario lo mudo: se queda donde esta
            QuitarHijo(r.abuelo, r.child);
            r.child->Parent = r.borrado;
            r.child->pos = r.posBajoBorrado;
            r.borrado->Childrens.push_back(r.child);
        }
        // restaurar las refs (el armature/target volvio a la escena): el modificador recupera su target y skinArmature
        // su puntero -> la malla vuelve a deformar como antes del borrado. La familia Target recupera puntero Y nombre.
        for (size_t i = 0; i < refs.size(); i++) RefEscribir(refs[i], true);
        // apagar la seleccion ACTUAL antes de restaurar la guardada (como SelectUndo):
        // sino un objeto seleccionado despues del borrado quedaba con select=true
        // fuera de ObjSelects (y el proximo Delete se lo llevaba puesto)
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = false;
        CameraActive = camPrev; ObjSelects = selPrev; ObjActivo = actPrev; CollectionActive = colPrev; // restaura seleccion + camara + coleccion activa
        {   // restaura la seleccion de animacion (el esqueleto o la vertex anim volvio). Es un
            // CAMINO MAS que cambia la anim activa: la seleccion del dope de las filas de vertex
            // anim se resuelve por la ACTIVA, asi que si la malla activa cambia hay que soltarla
            // (ver SoltarDopeSiCambioAnimVertex arriba).
            const int kindAntes = ActiveAnimKind; Mesh* const meshAntes = ActiveAnimMesh;
            ActiveAnimKind = animKindPrev; ActiveAnimArm = animArmPrev; ActiveAnimMesh = animMeshPrev;
            SoltarDopeSiCambioAnimVertex(kindAntes, meshAntes);
        }
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = true;
        DevolverAnimaciones();   // vuelve el objeto -> vuelve su animacion
        // NOMBRES (fallo F2): mientras los objetos estaban detachados, su nombre pudo
        // quedar TOMADO ("add cube / delete / add cube / undo" dejaba dos 'Cubo.001').
        // Se re-uniquifica el subarbol que vuelve, con la puerta de siempre (SetNameObj
        // se excluye a si mismo: si el nombre sigue libre no cambia nada). NO se arrastra
        // ningun vinculo: el que conserva el nombre es el que ya estaba en la escena, o
        // sea al que TODO lo que resuelve por nombre ya venia apuntando.
        for (size_t i = 0; i < ents.size(); i++) ReuniquificarSubarbol(ents[i].obj);
        enEscena = true;
    }
    // detachados: liberar de verdad. 'liberando' hace que el recorrido de abajo NO entre a este
    // comando mientras dura: sus objetos se estan destruyendo y no hay que tocarlos desde afuera
    // (ademas es un lazo: el comando todavia esta en el stack cuando corre su destructor, porque
    // Push hace 'delete g_undo.front()' ANTES del erase).
    // OJO: el SUBARBOL entero, no solo las raices (ver LiberarSubarbolDetachado arriba). Los
    // hijos que se PRESERVARON (reps) no estan adentro: Detachar ya los movio al abuelo, en la
    // escena viva, antes de que este comando se quedara con nada.
    ~DeleteUndo() {
        liberando = true;
        if (!enEscena) {
            std::set<Object*> hechos;
            // SOLO lo que este comando saco de verdad del arbol (e.mio): liberar una raiz que
            // quedo colgada en la escena es el use-after-free del fallo A.
            for (size_t i = 0; i < ents.size(); i++)
                if (ents[i].mio) LiberarSubarbolDetachado(ents[i].obj, hechos);
        }
    }
    // 'borrado' se libera: los subarboles que este comando tiene DETACHADOS son inalcanzables
    // desde SceneCollection, asi que el recorrido del destructor de Object no les llega y se
    // quedaban con el puntero muerto (y con la direccion reciclada, apuntando a OTRO objeto).
    void DesvincularDetachados(Object* borrado) {
        // el DUENO de una ref es un objeto que estaba VIVO al capturar (no es de este comando):
        // si lo liberan, RefEscribir escribiria sobre memoria muerta -o sobre el objeto que
        // reciclo la direccion-. La entrada se mata (dueno=NULL -> RefEscribir es no-op).
        for (size_t i = 0; i < refs.size(); i++)
            if (refs[i].dueno == borrado) { refs[i].dueno = NULL; refs[i].mod = NULL; }
        // idem para los hijos PRESERVADOS: el hijo y el abuelo NO son de este comando (viven en la
        // escena) y el borrado si lo es. Si cualquiera de los tres se libera, la replica queda MUERTA
        // (child=NULL -> los dos replays la saltean) en vez de deshacerse sobre memoria liberada.
        for (size_t i = 0; i < reps.size(); i++)
            if (reps[i].child == borrado || reps[i].abuelo == borrado || reps[i].borrado == borrado) {
                reps[i].child = NULL; reps[i].abuelo = NULL; reps[i].borrado = NULL; reps[i].movida = false;
            }
        if (liberando || enEscena) return;   // enEscena: sus objetos ya los camina el arbol vivo
        for (size_t i = 0; i < ents.size(); i++) {
            if (!ents[i].mio) continue;      // esa raiz sigue en la escena: la camina el arbol vivo
            std::vector<Object*> pila; pila.push_back(ents[i].obj);
            while (!pila.empty()) {
                Object* o = pila.back(); pila.pop_back();
                if (!o || o == borrado) continue;
                o->DesvincularDe(borrado);
                for (size_t k = 0; k < o->Childrens.size(); k++) pila.push_back(o->Childrens[k]);
            }
        }
    }
    // ents/reps/selPrev: (a) punteros a Object/Mesh, propiedad de ESTE comando mientras estan
    // detachados (y los que NO son suyos se sueltan en DesvincularDetachados, arriba).
    // refs: (a) pero OJO, no todos con el mismo argumento -y decir "todos revalidados" era FALSO
    // hasta la ronda 11-:
    //   - refs[].target es de ESTE comando (esta detachado adentro de ents) -> vive lo que vive
    //     el comando;
    //   - refs[].dueno NO es suyo: es un objeto que estaba vivo al capturar. Se suelta cuando lo
    //     destruyen (DesvincularDetachados de arriba) y con dueno=NULL RefEscribir es no-op;
    //   - refs[].mod TAMPOCO es suyo y ni siquiera es un Object: es un Modifier* del stack de una
    //     malla ajena, que ~Object NO enumera (no es una ref de objeto) y que se libera por su
    //     cuenta. Por eso se revalida al escribir, por PUNTERO + SERIAL, contra el stack de su
    //     malla (ModVivoEn / RefEscribir). Sin eso el undo escribia el target encima del
    //     modificador de OTRA malla y lo guardaba en el .w3d: test 'modtargetuaf'.
    // refs[].modSerial es (d) -una copia del numero de identidad-.
    // anims[].esc es (c): el INDICE de la escena en SceneAnimations, que es una lista de familia (2) -el "-"
    // de la tarjeta Animation borra una escena SIN empujar ningun paso de undo- y por eso se remapea abajo.
    // ANTES estaba clasificado como (a) "puntero revalidado contra SceneAnimations", y era falso: esa
    // revalidacion es la que Undo.h descarta para elementos de una lista (delete + new = direccion reciclada).
    // anims[].escNombre es (d) y anims[].a es (b) -una copia por valor de la AnimationObject-.
    // anims[].pos es (c) contra la lista de curvas de ESA escena: NO es una lista con nombre (no tiene
    // destinos de rename ni pasa por RemapEnStacks) y desde la ronda 6 nadie la indexa (KeyframesUndo va por
    // identidad de curva), asi que 'pos' es solo el ORDEN que ve el dope sheet y se clampea al insertar.
    // DelEntry::index es (c) -una posicion en Parent->Childrens- pero esa lista NO es una lista
    // con nombre: no tiene destinos de rename por indice ni pasa nunca por RemapEnStacks, y
    // ademas se RE-RESUELVE por puntero en cada Detachar() y se clampea al re-insertar. Si algun
    // dia el orden de los hijos se volviera indexable, este comando tiene que remapearla.
    void RemapLista(const W3dRenameDest& lista, int a, int b) {
        if (!W3dMismaLista(W3dDestGlobal(W3dRenameDest::SceneAnimG, -1), lista)) return;
        for (size_t i = 0; i < anims.size(); i++) {
            if (anims[i].esc < 0) continue;                       // ya huerfana
            if (!RemapIndiceGuardado(anims[i].esc, a, b)) anims[i].esc = -1;  // borraron SU escena
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
//  redo (Push -> LimpiarRedo) y no queda ningun comando "de ida" esperando un arbol viejo.
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
//  Aplicar() es el swap de siempre: el mismo codigo deshace y rehace.
//
//  NO-OP SEGURO (las dos formas de morir de esta familia, ver Undo.h)
//   - si el objeto o el padre guardado se LIBERAN, DesvincularDetachados mata la entrada. Ese
//     virtual es justo el que se omitio en la ronda 12 y produjo el bloqueante de aquella vez;
//   - si el objeto o el padre estan DETACHADOS (los tiene un DeleteUndo, fuera del arbol) no se
//     toca NADA: re-insertarlos meteria el mismo puntero en dos listas de hijos, que es
//     exactamente el bug que este comando viene a cerrar. Por eso se chequea que los dos sean
//     ALCANZABLES desde SceneCollection antes de mover nada.
// ============================================================================
class ReparentUndo : public UndoCmd {
    Object*    obj;
    Object*    padre;    // CRUDO: NULL = primer nivel (vive en SceneCollection->Childrens)
    int        idx;      // posicion entre los hermanos
    Vector3    pos, rotEuler, scale;
    Quaternion rot;
    bool       vivo;     // false = lo liberaron: no-op seguro
    static Object* ContDe(Object* padreCrudo) { return padreCrudo ? padreCrudo : SceneCollection; }
    static int IndiceEn(Object* cont, Object* o) {
        if (!cont) return -1;
        for (size_t i = 0; i < cont->Childrens.size(); i++) if (cont->Childrens[i] == o) return (int)i;
        return -1;
    }
    // alcanzable desde la raiz? (un objeto DETACHADO por un DeleteUndo no lo es)
    static bool EnEscena(Object* o) { return o && SceneCollection && EnSubarbol(SceneCollection, o); }
    void Leer() {
        padre = obj->Parent; idx = IndiceEn(ContDe(padre), obj);
        pos = obj->pos; rot = obj->Rot(); rotEuler = obj->rotEuler; scale = obj->scale;
    }
public:
    ReparentUndo(Object* o) : obj(o), padre(NULL), idx(-1), vivo(false) {
        if (!obj) return;
        vivo = true;
        Leer();
    }
    bool Vacio() const { return !vivo || !obj; }
    // hubo mudanza real? Un drag que suelta al objeto donde ya estaba no merece un paso.
    bool Difiere() const {
        if (Vacio()) return false;
        // se compara el CONTENEDOR, no el puntero crudo: los objetos de primer nivel nacen con
        // Parent == NULL y el embudo se lo normaliza a SceneCollection aunque no se muevan de
        // lugar. Son el mismo estado para todo el editor, y contarlo como cambio dejaba un paso
        // de undo que "no hace nada" en cada drag que suelta al objeto donde ya estaba.
        // (El undo igual RESTAURA el crudo, que es lo unico que devuelve el estado exacto.)
        if (ContDe(obj->Parent) != ContDe(padre)) return true;
        if (IndiceEn(ContDe(obj->Parent), obj) != idx) return true;
        if (obj->pos.x != pos.x || obj->pos.y != pos.y || obj->pos.z != pos.z) return true;
        if (obj->rotEuler.x != rotEuler.x || obj->rotEuler.y != rotEuler.y || obj->rotEuler.z != rotEuler.z) return true;
        if (obj->scale.x != scale.x || obj->scale.y != scale.y || obj->scale.z != scale.z) return true;
        return false;
    }
    void Aplicar() {
        if (Vacio()) return;
        Object* destino = ContDe(padre);
        Object* actual  = ContDe(obj->Parent);
        if (!destino || !actual) return;
        if (!EnEscena(obj) || !EnEscena(destino)) return;   // detachado: NO tocar (ver el bloque de arriba)
        const int idxLive = IndiceEn(actual, obj);
        if (idxLive < 0) return;                            // no esta en la lista de su padre: no-op
        Object* padreLive = obj->Parent;
        Vector3 pl = obj->pos, el = obj->rotEuler, sl = obj->scale; Quaternion rl = obj->Rot();
        actual->Childrens.erase(actual->Childrens.begin() + idxLive);
        int k = idx; if (k < 0) k = 0;
        if (k > (int)destino->Childrens.size()) k = (int)destino->Childrens.size();
        destino->Childrens.insert(destino->Childrens.begin() + k, obj);
        obj->Parent = padre;
        obj->pos = pos; obj->scale = scale;
        obj->SetRotSnapshot(rot, rotEuler);   // tal cual se capturo (igual que TransformUndo)
        padre = padreLive; idx = idxLive; pos = pl; rot = rl; rotEuler = el; scale = sl;
    }
    // el objeto o el padre se estan LIBERANDO: la entrada queda muerta (no-op) en vez de
    // aplicarse sobre memoria liberada -o sobre el objeto que reciclo la direccion-.
    void DesvincularDetachados(Object* borrado) {
        if (borrado && (borrado == obj || borrado == padre)) { vivo = false; obj = NULL; padre = NULL; }
    }
    // obj/padre: (a) punteros sueltos en DesvincularDetachados. pos/rot/rotEuler/scale: (d).
    // idx: es una POSICION, pero en Parent->Childrens, que NO es una lista con nombre (no tiene
    // destinos de rename por indice y nunca pasa por RemapEnStacks) - el MISMO argumento que
    // DelEntry::index de DeleteUndo -, y ademas se CLAMPEA al re-insertar. Si algun dia el orden
    // de los hijos se vuelve indexable, este comando necesita remapeo propio.
    W3D_UNDO_SIN_INDICES
};

// ---- stacks ----
static std::vector<UndoCmd*> g_undo;
static std::vector<UndoCmd*> g_redo;
static TransformUndo*        g_pendingT  = NULL; // transform de objeto en curso (sin confirmar)
static EditMoveUndo*         g_pendingEM = NULL; // move de malla en edit mode en curso
static MaterialModUndo*      g_pendingMat = NULL; // modificacion de material en curso (checkbox/shininess)
static ReparentUndo*         g_pendingRep = NULL; // mudanza de padre en curso (ver ReparentUndo)
static const size_t          MAXU = 100;
// bases de los GRUPOS abiertos (UndoGrupoIniciar/Fin): posiciones en g_undo. El desalojo del
// tope de abajo (MAXU) las corre, sino UndoGrupoFin fundiria pasos de ANTES del grupo.
static std::vector<size_t>   g_grupos;

// SACAR DEL STACK ANTES DE HACER EL delete, siempre. Destruir un comando LIBERA los objetos que
// tiene detachados, y eso dispara ~Object -> el recorrido de abajo, que camina g_undo/g_redo: si
// el comando que se esta destruyendo (o uno ya destruido de la misma tanda) sigue en el vector,
// el recorrido llama a un metodo virtual sobre memoria liberada. Es el mismo bug que estamos
// cerrando, un nivel mas arriba.
static void BorrarCmds(std::vector<UndoCmd*>& v) {
    std::vector<UndoCmd*> tmp; tmp.swap(v);          // el vector queda VACIO antes del primer delete
    for (size_t i = 0; i < tmp.size(); i++) delete tmp[i];
}
static void LimpiarRedo() { BorrarCmds(g_redo); }

// desaloja el fondo del stack hasta respetar MAXU. Las bases de los grupos ABIERTOS son
// posiciones absolutas en g_undo: si no se corren con el desalojo, UndoGrupoFin funde de mas.
static void DesalojarViejos() {
    while (g_undo.size() > MAXU) {
        UndoCmd* viejo = g_undo.front();
        g_undo.erase(g_undo.begin());   // fuera del stack ANTES del delete (ver BorrarCmds)
        delete viejo;
        for (size_t i = 0; i < g_grupos.size(); i++) if (g_grupos[i] > 0) g_grupos[i]--;
    }
}

static void Push(UndoCmd* c) {
    if (!c) return;
    LimpiarRedo(); // una accion NUEVA invalida el redo
    g_undo.push_back(c);
    DesalojarViejos();
}

// ============================================================================
//  EL DESVINCULADO DEL DESTRUCTOR TIENE QUE ALCANZAR TAMBIEN A LO DETACHADO
//
//  ~Object suelta las referencias caminando el ARBOL VIVO (DesvincularDelArbol, Objects.cpp).
//  Eso deja afuera a los objetos que los comandos del undo tienen DETACHADOS: no cuelgan de
//  SceneCollection, asi que nadie les avisa y se quedan con un puntero a memoria liberada. Con
//  la direccion reciclada por el allocator la referencia pasa a apuntar a OTRO objeto, en
//  silencio (es el modo de fallar de esta familia entera de bugs).
//
//  Esto NO reemplaza a RecolectarRefs: son dos cosas distintas y hacen falta las dos.
//   - RecolectarRefs/RefEscribir es la parte del MODELO: al borrar, el que referencia suelta
//     puntero Y nombre (asi la instancia deja de dibujar al borrado y el .w3d no queda con un
//     target a un objeto ausente), y al deshacer los recupera. Eso el destructor no lo puede
//     hacer: cuando llega el free ya no hay nada que restaurar.
//   - esto de aca es la parte de MEMORIA, y es un piso: garantiza que NINGUN puntero sobreviva
//     al free venga de donde venga. Hoy, con el desalojo FIFO del stack, el caso "referenciador
//     detachado que sobrevive a su target liberado" es dificil de armar; pero esa es una
//     propiedad del ORDEN DE DESALOJO de hoy, no del diseno, y no quiero que la seguridad de
//     memoria dependa de ella.
// ============================================================================
static void UndoDesvincularDetachados(Object* borrado) {
    for (size_t i = 0; i < g_undo.size(); i++) g_undo[i]->DesvincularDetachados(borrado);
    for (size_t i = 0; i < g_redo.size(); i++) g_redo[i]->DesvincularDetachados(borrado);
}
// el gancho se pone solo al cargar el binario: no hay un "init del undo" al que sumarse, y
// olvidarse de llamarlo seria justo el agujero que esto tapa. La lista de ganchos arranca en
// NULL por inicializacion ESTATICA (constante), o sea antes que cualquier constructor global.
struct UndoEngancharDesvincular {
    UndoEngancharDesvincular() { W3dDesvincularRegistrar(UndoDesvincularDetachados); }
};
static UndoEngancharDesvincular g_undoEngancheDesvincular;

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
// mismo bug de siempre pero un rato antes, y un RemapLista impecable no lo tapa. Hoy no se
// conoce un camino de UI que corra una lista con un pendiente abierto (los Iniciar/Confirmar
// se cierran dentro de la misma operacion), pero eso es una propiedad de la UI de HOY, no del
// undo. Se define al FINAL del archivo: necesita ver TODOS los pendientes, y cada uno se
// declara al lado de su comando.
static void RemapEnPendientes(const W3dRenameDest& lista, int a, int b);
static void RemapEnStacks(const W3dRenameDest& lista, int a, int b) {
    for (size_t i = 0; i < g_undo.size(); i++) g_undo[i]->RemapLista(lista, a, b);
    for (size_t i = 0; i < g_redo.size(); i++) g_redo[i]->RemapLista(lista, a, b);
    RemapEnPendientes(lista, a, b);
}
static bool CapaSwapEnMalla(Mesh* m, int capa, int a, int b) {
    if (!m || a == b) return false;
    if (capa == W3dRenameDest::VGroup) {
        if (a < 0 || b < 0 || a >= (int)m->vertexGroups.size() || b >= (int)m->vertexGroups.size()) return false;
        VertexGroup* t = m->vertexGroups[a]; m->vertexGroups[a] = m->vertexGroups[b]; m->vertexGroups[b] = t;
        return true;
    }
    if (capa == W3dRenameDest::UVGroup) {
        if (a < 0 || b < 0 || a >= (int)m->uvGroups.size() || b >= (int)m->uvGroups.size()) return false;
        UVGroup* t = m->uvGroups[a]; m->uvGroups[a] = m->uvGroups[b]; m->uvGroups[b] = t;
        return true;
    }
    if (capa == W3dRenameDest::UVMap) {
        if (a < 0 || b < 0 || a >= (int)m->uvMaps.size() || b >= (int)m->uvMaps.size()) return false;
        UVMap* t = m->uvMaps[a]; m->uvMaps[a] = m->uvMaps[b]; m->uvMaps[b] = t;
        return true;
    }
    if (capa == W3dRenameDest::ColorLayer) {
        if (a < 0 || b < 0 || a >= (int)m->colorLayers.size() || b >= (int)m->colorLayers.size()) return false;
        ColorLayer* t = m->colorLayers[a]; m->colorLayers[a] = m->colorLayers[b]; m->colorLayers[b] = t;
        return true;
    }
    return false;   // Arm2D / VertAnim: no tienen botones de reordenar
}
static int* CapaActivoDe(Mesh* m, int capa) {
    if (!m) return NULL;
    if (capa == W3dRenameDest::VGroup)     return &m->grupoActivo;
    if (capa == W3dRenameDest::UVGroup)    return &m->uvGrupoActivo;
    if (capa == W3dRenameDest::UVMap)      return &m->uvMapActivo;
    if (capa == W3dRenameDest::ColorLayer) return &m->colorActivo;
    return NULL;
}
// las capas POR CORNER (uv maps / capas de color) se hornean a los arrays de render segun cual
// es la ACTIVA: reordenar mueve el indice activo, asi que hay que re-hornear (los llamadores de
// los botones ya lo hacian; el UNDO tambien tiene que hacerlo o el viewport queda con la capa
// anterior). Las otras dos listas no se hornean: solo se invalida el cache de skinning.
static void CapaPostCambio(Mesh* m, int capa) {
    if (!m) return;
    if (capa == W3dRenameDest::UVMap || capa == W3dRenameDest::ColorLayer) m->AplicarCapasAlRender();
    m->lastSkinFrame = -999999; m->pose2dDirty = true;
}
class CapaOrdenUndo : public UndoCmd {
    Mesh* m; int capa, a, b, activo;
public:
    CapaOrdenUndo(Mesh* M, int C, int A, int B, int Act) : m(M), capa(C), a(A), b(B), activo(Act) {}
    void Aplicar() {
        if (!m || !ObjetoEnEscena(m)) return;             // la malla ya no esta: no-op SEGURO
        if (!CapaSwapEnMalla(m, capa, a, b)) return;      // la lista cambio de tamano: no tocar
        int* act = CapaActivoDe(m, capa);
        if (act) { int cur = *act; *act = activo; activo = cur; }  // swap (sirve de ida y de vuelta)
        RemapEnStacks(W3dDestCapaMalla(m, capa, -1), a, b); // los renames capturados siguen a SU elemento
        CapaPostCambio(m, capa);
    }
    // ESTE PASO TAMBIEN GUARDA INDICES. No alcanza con remapear los destinos de rename: un
    // paso de reordenar es "intercambiar las posiciones a y b", y si entre medio se BORRA un
    // elemento de la lista sin que ese borrado sea deshacible, las posiciones se corren y el
    // Ctrl+Z termina intercambiando el PAR EQUIVOCADO (no corrompe nombres, pero deja un
    // orden que nunca existio). Ver el criterio general en Undo.h.
    void RemapLista(const W3dRenameDest& lista, int x, int y) {
        if (!W3dMismaLista(W3dDestCapaMalla(m, capa, -1), lista)) return;
        if (a < 0 || b < 0) return;                    // el paso ya estaba muerto
        // el activo guardado corre igual; si borraron JUSTO al que era el activo, ese indice ya
        // no significa nada (dejarlo apuntaria al VECINO, y el undo elegiria un activo raro).
        if (!RemapIndiceGuardado(activo, x, y)) activo = -1;
        if (!RemapIndiceGuardado(a, x, y) || !RemapIndiceGuardado(b, x, y))
            { a = -1; b = -1; }                        // borraron una de las dos puntas: el paso MUERE (no-op)
    }
};
bool UndoMoverCapaMalla(Mesh* m, int capa, int i, int j) {
    if (!m || i == j) return false;
    int* act = CapaActivoDe(m, capa);
    if (!act) return false;
    const int actAntes = *act;
    if (!CapaSwapEnMalla(m, capa, i, j)) return false;
    *act = j;                                 // el activo VIAJA con el elemento que se movio
    RemapEnStacks(W3dDestCapaMalla(m, capa, -1), i, j);
    Push(new CapaOrdenUndo(m, capa, i, j, actAntes));
    CapaPostCambio(m, capa);
    return true;
}

// ---- CLIPS 3D (Armature::animations) ----------------------------------------------------
// DECISION: la lista vive en el CORE (SkeletalAnimation.cpp, que compila tambien para el
// runtime de los juegos, donde no hay undo ni stacks que remapear) y el remapeo es del EDITOR.
// En vez de meterle al Core una dependencia con el undo (o un hook con puntero a funcion), el
// swap se hace ACA -exactamente como CapaSwapEnMalla hace el de las capas de la malla- y
// MoverAnimacionActiva del Core queda SOLO para el runtime / la carga. El editor (tarjeta
// Animation y el harness de tests) llama UndoMoverClipArm. No hace falta ningun .cpp nuevo.
static bool ClipSwapEnArm(Armature* a, int i, int j) {
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
// numero ANTES del dueno y DopeClavePartir exige el digito pegado al prefijo. Eso corria los indices de
// clip de las claves de TODOS los armatures, y el comentario que lo declaraba inofensivo ("el resolver
// chequea ademas el nombre del rig -> una clave ajena no resuelve ni antes ni despues") era FALSO: la
// clave ajena no resuelve MIENTRAS el rig activo sea otro, pero el usuario vuelve a SU rig y ahi la clave
// YA CORRIDA resuelve al CLIP EQUIVOCADO de su propio rig (test 'dopeclip3d'). El formato de la clave se
// dio vuelta a "arm:<rig>/k<CLIP>/..." (dueno primero, como el 2D) y el aviso ahora se acota:
static void DopeRemapClip3D(const Armature* a, int i, int j) {
    if (!a) return;
    DopeRemapIndiceClave("arm:" + DopeIdDueno(a) + "/k", i, j);   // el dueno POR SERIAL (el nombre se recicla)
}
class ClipOrdenUndo : public UndoCmd {
    Armature* a; int i, j, activo;
public:
    ClipOrdenUndo(Armature* A, int I, int J, int Act) : a(A), i(I), j(J), activo(Act) {}
    void Aplicar() {
        if (!a || !ObjetoEnEscena(a)) return;      // el armature ya no esta: no-op SEGURO
        if (!ClipSwapEnArm(a, i, j)) return;       // la lista cambio de tamano: no tocar
        int cur = a->animActiva; a->animActiva = activo; activo = cur;  // swap (ida y vuelta)
        RemapEnStacks(W3dDestClipArm(a, -1), i, j);
        DopeRemapClip3D(a, i, j);                  // las claves del dope llevan el clip adentro (ver arriba)
    }
    // idem CapaOrdenUndo::RemapLista (este paso guarda indices, no identidades)
    void RemapLista(const W3dRenameDest& lista, int x, int y) {
        if (!W3dMismaLista(W3dDestClipArm(a, -1), lista)) return;
        if (i < 0 || j < 0) return;                    // el paso ya estaba muerto
        if (!RemapIndiceGuardado(activo, x, y)) activo = -1; // borraron al que era el activo (ver CapaOrdenUndo)
        if (!RemapIndiceGuardado(i, x, y) || !RemapIndiceGuardado(j, x, y))
            { i = -1; j = -1; }                        // borraron una de las dos puntas: el paso MUERE
    }
};
bool UndoMoverClipArm(Armature* a, int i, int j) {
    if (!a || i == j) return false;
    const int actAntes = a->animActiva;
    if (!ClipSwapEnArm(a, i, j)) return false;
    a->animActiva = j;                        // el activo VIAJA con el clip que se movio
    RemapEnStacks(W3dDestClipArm(a, -1), i, j);
    DopeRemapClip3D(a, i, j);
    Push(new ClipOrdenUndo(a, i, j, actAntes));
    return true;
}

// ---- BORRADO: la OTRA mitad (borrar tambien corre los indices) ---------------------------
// Ver el bloque (B) de Undo.h: esto es SOLO para las listas cuyo "-" no empuja ningun paso de
// undo. Las que si lo empujan (las cuatro capas de la malla via UndoCapturarMallaGeo, los
// armatures 2D via UndoArm2DBorrar, los mesh parts) NO tienen que avisar: su Ctrl+Z restaura
// la lista entera con los indices originales y este desplazamiento los dejaria corridos.
void UndoListaBorrada(const W3dRenameDest& lista) {
    if (lista.i < 0) return;                  // nada que borrar (indice fuera de rango)
    RemapEnStacks(lista, lista.i, -1);        // b < 0 = borrado del indice a
}
void UndoListaMovida(const W3dRenameDest& lista, int i, int j) {
    if (i == j || i < 0 || j < 0) return;
    RemapEnStacks(lista, i, j);               // b >= 0 = swap a <-> b
}
// atajos: aviso + borrado en UN solo lugar (ver Undo.h). NO hacen el borrado deshacible: eso
// es una funcionalidad aparte (habria que quedarse con el clip vivo, como UndoArm2DBorrar).
// Lo que arreglan es que el rename ANTERIOR no escriba en el clip equivocado.
// ...y el mismo aviso a la SELECCION DEL DOPE, que tambien lleva el indice del clip / de la escena adentro
// de su clave desde la ronda 7 (ver el bloque de identidad en Timeline.cpp).
void UndoBorrarClipArm(Armature* a) {
    if (!a) return;
    const int i = a->animActiva;
    if (i >= 0 && i < (int)a->animations.size()) { UndoListaBorrada(W3dDestClipArm(a, i)); DopeRemapClip3D(a, i, -1); }
    BorrarAnimacionActiva(a);
}
void UndoBorrarClip2D(Mesh* m) {
    if (!m) return;
    Armature2D* arm = m->Arm2DActivoP();
    const int i = arm ? m->Arm2DAnimActiva() : -1;
    if (arm && i >= 0 && i < (int)m->Arm2DAnims().size()) {
        UndoListaBorrada(W3dDestClip2D(m, arm, i));
        char sa[48]; sprintf(sa, "/a%d/k", m->armature2dActivo);
        DopeRemapIndiceClave("arm2d:" + DopeIdDueno(m) + sa, i, -1);
    }
    Arm2DBorrarAnimacionActiva(m);
}
// ANIMACIONES DE ESCENA (SceneAnimations, lista GLOBAL del proyecto): el "-" de la tarjeta
// Animation cuando lo activo NO es un clip. Mismo caso que los clips (el borrado no es
// deshacible) y el rename SI se captura por indice (W3dDestGlobal(SceneAnimG, SceneAnimActiva),
// Properties.cpp) -> sin el aviso, borrar una escena de ABAJO dejaba el Ctrl+Z de un rename
// anterior escribiendo el nombre viejo encima de la escena de al lado.
// OJO CON EL CASO DE UNA SOLA ESCENA: ahi el Core NO borra nada (BorrarEscenaActiva solo vacia
// las curvas de la unica escena, que siempre queda), asi que avisar correria los indices SIN
// borrado y romperia justo al reves. Por eso el aviso va condicionado a size() > 1.
void UndoBorrarEscenaActiva() {
    InitSceneAnimations();
    const int i = SceneAnimActiva;
    if ((int)SceneAnimations.size() > 1 && i >= 0 && i < (int)SceneAnimations.size()) {
        UndoListaBorrada(W3dDestGlobal(W3dRenameDest::SceneAnimG, i));
        DopeRemapIndiceClave("obj:e", i, -1);   // las claves del dope llevan la escena adentro (ver Timeline.h)
    }
    BorrarEscenaActiva();
}

// ============================================================================
//  JOIN (Ctrl+J): comando ATOMICO = geometria del activo (MeshGeoUndo) + borrado de los mergeados (DeleteUndo).
//  Un solo Ctrl+Z restaura la geo del activo Y re-inserta los objetos mergeados (sin estado intermedio roto).
// ============================================================================
class JoinUndo : public ContenedorUndo {
    MeshGeoUndo* geo; DeleteUndo* del;
protected:
    // contenedor: no guarda indices PROPIOS, pero sus partes SI (el MeshGeoUndo lleva el
    // paralelo a las vertex anims, el DeleteUndo es DUENO de las mallas mergeadas) -> todos
    // los virtuales se reenvian, y eso lo hace ContenedorUndo con esta lista.
    // Aplicar: dos toggles independientes (geo del activo / arbol de escena), en este orden.
    void Partes(std::vector<UndoCmd*>& out){ out.push_back(geo); out.push_back(del); }
public:
    JoinUndo(MeshGeoUndo* g, DeleteUndo* d) : geo(g), del(d) {}
    ~JoinUndo() { delete geo; delete del; }
};
static MeshGeoUndo* g_pendingJoin = NULL; // geo del activo capturada por UndoJoinIniciar (antes del merge)

void UndoJoinIniciar(Mesh* activeMesh) {
    delete g_pendingJoin;
    g_pendingJoin = activeMesh ? new MeshGeoUndo(activeMesh) : NULL;
}
void UndoJoinConfirmar() {
    if (!g_pendingJoin) return;
    // los objetos a borrar son los que quedaron con select=true (el caller dejo SOLO los mergeados marcados).
    DeleteUndo* del = new DeleteUndo(false); // detacha (sin liberar) + guarda para deshacer
    Push(new JoinUndo(g_pendingJoin, del));  // 1 comando atomico (geo + borrado)
    g_pendingJoin = NULL;                    // ahora lo posee el JoinUndo
}

// ARMATURE: snapshot de la REST (T/R/S/preRot/rotOrder/tlNode/head/tail/pose) de todos los huesos, para deshacer un
// Apply Transform sobre el armature (que la hornea). Aplicar() = SWAP + invalida el cache de skin de las mallas del rig.
struct BoneRestEst { Vector3 restT, restR, restS, preRot, head, tail, poseT, poseR, poseS; int rotOrder; Matrix4 tlNode; };
class ArmatureBonesUndo : public UndoCmd {
    Armature* a; std::vector<BoneRestEst> e;
public:
    ArmatureBonesUndo(Armature* arm) : a(arm) {
        if (!a) return;
        for (size_t i=0;i<a->bones.size();i++){ W3dBone& b=a->bones[i];
            BoneRestEst t; t.restT=b.restT; t.restR=b.restR; t.restS=b.restS; t.preRot=b.preRot; t.rotOrder=b.rotOrder;
            t.tlNode=b.tlNode; t.head=b.head; t.tail=b.tail; t.poseT=b.poseT; t.poseR=b.poseR; t.poseS=b.poseS;
            e.push_back(t); }
    }
    bool Vacio() const { return e.empty(); }
    void Aplicar() {
        if (!a || e.size()!=a->bones.size()) return;
        for (size_t i=0;i<e.size();i++){ W3dBone& b=a->bones[i]; BoneRestEst& t=e[i];
            Vector3 rt=b.restT,rr=b.restR,rs=b.restS,pr=b.preRot,hd=b.head,tl=b.tail,pt=b.poseT,pR=b.poseR,pS=b.poseS; int ro=b.rotOrder; Matrix4 tn=b.tlNode;
            b.restT=t.restT; b.restR=t.restR; b.restS=t.restS; b.preRot=t.preRot; b.rotOrder=t.rotOrder; b.tlNode=t.tlNode;
            b.head=t.head; b.tail=t.tail; b.poseT=t.poseT; b.poseR=t.poseR; b.poseS=t.poseS;
            t.restT=rt; t.restR=rr; t.restS=rs; t.preRot=pr; t.rotOrder=ro; t.tlNode=tn; t.head=hd; t.tail=tl; t.poseT=pt; t.poseR=pR; t.poseS=pS; }
        a->poseSerial++; // la pose se recalcula (rest nueva)
        InvalidarSkinDeArmature(a); // libera el cache stale + fuerza re-skin (la firma del cache no ve la rest)
    }
    // e[] es (c): vector PARALELO a Armature::bones. Esa lista es familia (1) -TODA edicion de
    // huesos empuja un BonesUndo, que la restaura entera, asi que por LIFO los indices vuelven
    // solos- y ademas Aplicar tiene guard de tamano (e.size()!=bones.size() -> no-op). Nadie
    // llama RemapEnStacks con la lista de huesos; si algun dia pasara, hay que remapear aca.
    W3D_UNDO_SIN_INDICES
};

// ============================================================================
//  APPLY (Alt+A): comando ATOMICO = geometria (MeshGeoUndo por malla) + transform (TransformUndo de los
//  seleccionados) + rest de huesos (ArmatureBonesUndo por armature). Un Ctrl+Z restaura la geo horneada, los
//  pos/rot/scale reseteados Y la rest de los huesos en 1 solo paso.
// ============================================================================
class ApplyUndo : public ContenedorUndo {
    std::vector<MeshGeoUndo*> geos; TransformUndo* xf; std::vector<ArmatureBonesUndo*> arms;
protected:
    // contenedor: PROPAGA TODOS los virtuales via ContenedorUndo (los MeshGeoUndo de adentro
    // llevan el paralelo a las vertex anims). Orden de Aplicar: geos, transform, rest de huesos
    // (son toggles independientes).
    void Partes(std::vector<UndoCmd*>& out){
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

void UndoApplyIniciar() {
    for (size_t i=0;i<g_pendingApplyGeos.size();i++) delete g_pendingApplyGeos[i];
    g_pendingApplyGeos.clear();
    for (size_t i=0;i<g_pendingApplyArms.size();i++) delete g_pendingApplyArms[i];
    g_pendingApplyArms.clear();
    delete g_pendingApplyXf; g_pendingApplyXf = new TransformUndo(); // snapshot de pos/rot/scale de los seleccionados
    for (size_t i=0;i<ObjSelects.size();i++){ Object* o=ObjSelects[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh) g_pendingApplyGeos.push_back(new MeshGeoUndo((Mesh*)o));
        else if (o->getType()==ObjectType::armature) g_pendingApplyArms.push_back(new ArmatureBonesUndo((Armature*)o)); }
}
void UndoApplyConfirmar() {
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
    Armature* a; std::vector<PoseEst> e;
public:
    PoseUndo(Armature* arm) : a(arm) {
        if (!a) return;
        for (size_t i=0;i<a->bones.size();i++){ PoseEst p; p.T=a->bones[i].poseT; p.R=a->bones[i].poseR; p.S=a->bones[i].poseS; e.push_back(p); }
    }
    bool Vacio() const { return e.empty(); }
    bool Cambio() const { // hubo cambio real respecto al snapshot? (evita empujar un undo vacio si G+Esc / click sin mover)
        if (!a || e.size()!=a->bones.size()) return false;
        for (size_t i=0;i<e.size();i++){ const W3dBone& b=a->bones[i];
            if (b.poseT.x!=e[i].T.x||b.poseT.y!=e[i].T.y||b.poseT.z!=e[i].T.z) return true;
            if (b.poseR.x!=e[i].R.x||b.poseR.y!=e[i].R.y||b.poseR.z!=e[i].R.z) return true;
            if (b.poseS.x!=e[i].S.x||b.poseS.y!=e[i].S.y||b.poseS.z!=e[i].S.z) return true; }
        return false;
    }
    void Aplicar(){
        if (!a || e.size()!=a->bones.size()) return;
        for (size_t i=0;i<e.size();i++){ W3dBone& b=a->bones[i]; Vector3 t=b.poseT,r=b.poseR,s=b.poseS;
            b.poseT=e[i].T; b.poseR=e[i].R; b.poseS=e[i].S; e[i].T=t; e[i].R=r; e[i].S=s; }
        a->poseDirty=true; a->poseSerial++; InvalidarSkinDeArmature(a);
    }
    W3D_UNDO_SIN_INDICES // e[]: (c) paralelo a Armature::bones, mismo caso que ArmatureBonesUndo
                         // (familia (1) + guard de tamano). a: (a) Armature*.
};
// ============================================================================
//  KEYFRAMES (dope sheet / editor de curvas / tarjeta "Keyframe"): snapshot de TODAS las curvas
//  de la animacion que se esta editando (escena, clip 3D, clip del armature 2D o vertex anim de
//  objeto). Ctrl+Z devuelve los keyframes.
//
//  *** LA LISTA VA POR IDENTIDAD, NO "LA QUE ESTE ACTIVA AL DESHACER" (fallo (5)) ***
//  Antes el snapshot era un vector PARALELO POSICIONAL y la lista de curvas se RE-RESOLVIA AL
//  APLICAR desde los GLOBALES (ActiveAnimKind / ActiveAnimArm->animActiva / currentAnim /
//  AnimationObjects), con un unico guard "c.size() == snap.size()" -que es justo lo que Undo.h
//  llama "la version silenciosa del bug"-. O sea: el Ctrl+Z escribia en la lista que estuviera
//  activa EN ESE MOMENTO, no en la que se edito. Tres caminos de UI normales lo rompian, los
//  tres sin crash y sin aviso:
//    - editar un keyframe del clip K0 y despues elegir K1 en la tarjeta Animation (animActiva=1;
//      dos clips de igual estructura pasan el guard): el Ctrl+Z metia las curvas de K0 en K1;
//    - editar el key de un objeto, BORRAR ese objeto y deshacer el borrado: AnimationObjects
//      volvia en otro orden y las curvas quedaban CRUZADAS entre objetos;
//    - editar en la escena 1 y elegir la escena 0 (SetEscenaActiva hace swap de TODA la lista):
//      el Ctrl+Z escribia las curvas de una escena ENCIMA de las de la otra.
//  Ahora el paso guarda DOS identidades y no mira ningun conteo:
//    (1) DE QUE LISTA salio: kind + dueno concreto + cual clip/anim/escena (KFLista). Se resuelve
//        ESA al aplicar; si ya no existe, no-op seguro. El indice de esa lista es un miembro (c)
//        y se remapea (RemapLista) como cualquier otro.
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
//    kind 0 -> SceneAnimations           (familia (2): UndoBorrarEscenaActiva avisa)
//    kind 1 -> Armature::animations      (familia (2): UndoBorrarClipArm / UndoMoverClipArm avisan)
//    kind 3 -> Mesh::animations          (familia (2): el "-" de la vertex anim avisa)
//    kind 4 -> Armature2D::anims         (familia (2): UndoBorrarClip2D avisa)
struct KFLista {
    int kind;
    Armature*   arm;     // kind 1
    Mesh*       mesh;    // kind 3 y 4
    Armature2D* arm2d;   // kind 4: el rig 2D CONCRETO (la lista de la malla puede cambiar)
    int idx;
    KFLista() : kind(0), arm(0), mesh(0), arm2d(0), idx(-1) {}
};
// ---- (2) IDENTIDAD DE UNA CURVA DENTRO DE LA LISTA ---------------------------------------
// obj  : kind 0, el objeto animado (AnimationObject::obj). NUNCA se desreferencia: solo se
//        compara contra los punteros vivos de la lista.
// bone : kind 1 y 4, el hueso del track (BoneTrack::bone / Bone2DTrack::bone).
// prop/comp: la curva (AnimPosition/AnimRotation/..., AnimX/Y/Z).
struct KFId {
    Object* obj; int bone; int prop, comp;
    KFId() : obj(0), bone(-1), prop(-1), comp(-1) {}
    bool operator==(const KFId& o) const {
        return obj == o.obj && bone == o.bone && prop == o.prop && comp == o.comp;
    }
};
struct KFCurva { std::vector<keyFrame>* c; KFId id; };
static void KFAgregar(std::vector<KFCurva>& out, std::vector<AnimProperty>& props, Object* obj, int bone){
    for (size_t p = 0; p < props.size(); p++){
        KFCurva k; k.c = &props[p].keyframes;
        k.id.obj = obj; k.id.bone = bone; k.id.prop = props[p].Property; k.id.comp = props[p].component;
        out.push_back(k);
    }
}
// la lista de curvas que el timeline esta editando AHORA (para capturar)
static void KFIdentidadActiva(KFLista& id){
    id = KFLista();
    if (ActiveAnimKind == 1 && ActiveAnimArm){
        id.kind = 1; id.arm = ActiveAnimArm; id.idx = ActiveAnimArm->animActiva;
    } else if (ActiveAnimKind == 4 && ActiveAnimMesh){
        id.kind = 4; id.mesh = ActiveAnimMesh; id.arm2d = ActiveAnimMesh->Arm2DActivoP();
        id.idx = id.arm2d ? ActiveAnimMesh->Arm2DAnimActiva() : -1;
    } else if (ActiveAnimKind == 3 && ActiveAnimMesh){
        id.kind = 3; id.mesh = ActiveAnimMesh;
        VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
        id.idx = va ? va->currentAnim : -1;
    } else {                                   // escena (y modo juego, que anima las mismas curvas)
        InitSceneAnimations();
        id.kind = 0; id.idx = SceneAnimActiva;
    }
}
// resuelve LA LISTA GUARDADA (no la activa) -> sus curvas vivas + la identidad de cada una.
// Sale VACIO si esa lista ya no existe: el paso queda en no-op seguro.
static void KFResolver(const KFLista& id, std::vector<KFCurva>& out){
    out.clear();
    if (id.idx < 0) return;
    if (id.kind == 1){
        if (!id.arm || !ObjetoEnEscena(id.arm)) return;
        if (id.idx >= (int)id.arm->animations.size()) return;
        SkeletalAnimation* an = id.arm->animations[id.idx]; if (!an) return;
        for (size_t t = 0; t < an->tracks.size(); t++) KFAgregar(out, an->tracks[t].Propertys, NULL, an->tracks[t].bone);
    } else if (id.kind == 4){
        if (!id.mesh || !ObjetoEnEscena(id.mesh) || !Arm2DEnMalla(id.mesh, id.arm2d)) return;
        if (id.idx >= (int)id.arm2d->anims.size()) return;
        Armature2DAnimation* an = id.arm2d->anims[id.idx]; if (!an) return;
        for (size_t t = 0; t < an->tracks.size(); t++) KFAgregar(out, an->tracks[t].Propertys, NULL, an->tracks[t].bone);
    } else if (id.kind == 3){
        if (!id.mesh || !ObjetoEnEscena(id.mesh)) return;
        if (id.idx >= (int)id.mesh->animations.size()) return;
        VertexAnimation* an = id.mesh->animations[id.idx]; if (!an) return;
        KFAgregar(out, an->curvas, NULL, -1);
    } else {
        InitSceneAnimations();
        if (id.idx >= (int)SceneAnimations.size()) return;
        // las curvas de la escena ACTIVA viven en el global AnimationObjects; las de las demas,
        // guardadas en su SceneAnimation (SetEscenaActiva hace swap entre las dos).
        std::vector<AnimationObject>& lst = (id.idx == SceneAnimActiva)
                                          ? AnimationObjects : SceneAnimations[id.idx]->objetos;
        for (size_t i = 0; i < lst.size(); i++) KFAgregar(out, lst[i].Propertys, lst[i].obj, -1);
    }
}
static int KFBuscar(const std::vector<KFCurva>& c, const KFId& id){
    for (size_t i = 0; i < c.size(); i++) if (c[i].id == id) return (int)i;
    return -1;
}
class KeyframesUndo : public UndoCmd {
    struct KFGuardada { KFId id; std::vector<keyFrame> kf; };
    std::vector<KFGuardada> snap;
    KFLista lid;                 // de QUE lista de curvas salio el snapshot
    // snapshot de los FRAMES de vertices de la anim de objeto activa (kind 3). Se guarda
    // como BLOB binario (VertexAnimSerializar): es una copia por valor autocontenida, sin
    // gestion manual de los buffers -> imposible doble-free/fuga. Va POR (malla + indice)
    // -que es la MISMA identidad de lista de arriba, lid.mesh/lid.idx- y no por un
    // VertexAnimation* crudo: el "-" de la tarjeta hace delete y el proximo new puede caer en
    // LA MISMA direccion (ver Undo.h). Ese indice es (c) y lo remapea RemapLista.
    std::vector<unsigned char> vfBlob; bool vfHay;
    VertexAnimation* VfAnim() const {
        if (lid.kind != 3 || !lid.mesh) return NULL;
        if (lid.idx < 0 || lid.idx >= (int)lid.mesh->animations.size()) return NULL;
        return lid.mesh->animations[lid.idx];
    }
public:
    KeyframesUndo(){
        KFIdentidadActiva(lid);
        std::vector<KFCurva> c; KFResolver(lid, c);
        snap.resize(c.size());
        for (size_t i = 0; i < c.size(); i++){ snap[i].id = c[i].id; snap[i].kf = *c[i].c; }
        vfHay = false;
        VertexAnimation* van = VfAnim();
        if (van && van->target){ VertexAnimSerializar(*van, vfBlob); vfHay = true; }
    }
    bool Vacio() const { return snap.empty() && !vfHay; }
    bool Cambio() const { // hubo cambio real? (no empujar un undo vacio)
        std::vector<KFCurva> c; KFResolver(lid, c);
        std::vector<char> visto(c.size(), 0);
        for (size_t i = 0; i < snap.size(); i++){
            const int k = KFBuscar(c, snap[i].id);
            if (k < 0){ if (!snap[i].kf.empty()) return true; continue; } // la curva ya no existe
            visto[k] = 1;
            const std::vector<keyFrame>& viva = *c[k].c; const std::vector<keyFrame>& g = snap[i].kf;
            if (viva.size() != g.size()) return true;
            for (size_t q = 0; q < g.size(); q++){
                const keyFrame& a = viva[q]; const keyFrame& b = g[q];
                // OJO: tambien la INTERPOLACION y los HANDLES. Curvar un tramo no mueve el keyframe (mismo frame y
                // mismo valor): si solo se miraban esos dos, el undo de una curva se descartaba por "no hubo cambio".
                if (a.frame != b.frame || a.value != b.value) return true;
                if (a.Interpolation != b.Interpolation || a.handleType != b.handleType) return true;
                if (a.inDF != b.inDF || a.inDV != b.inDV || a.outDF != b.outDF || a.outDV != b.outDV) return true;
            }
        }
        for (size_t k = 0; k < c.size(); k++)      // curva NUEVA con keyframes (la creo la operacion)
            if (!visto[k] && !c[k].c->empty()) return true;
        // FRAMES de vertices: comparar el blob de ahora contra el snapshot (asi un cambio que
        // fue SOLO de vertices -mover/borrar/interp/insertar- tambien empuja undo)
        if (vfHay){ VertexAnimation* v = VfAnim();
            if (v && v->target){ std::vector<unsigned char> now; VertexAnimSerializar(*v, now); if (now != vfBlob) return true; } }
        return false;
    }
    void Aplicar(){
        std::vector<KFCurva> c; KFResolver(lid, c);   // LA lista guardada, no la activa
        std::vector<char> visto(c.size(), 0);
        for (size_t i = 0; i < snap.size(); i++){
            const int k = KFBuscar(c, snap[i].id);
            if (k < 0) continue;                      // esa curva ya no existe: la entrada queda guardada
            visto[k] = 1;
            c[k].c->swap(snap[i].kf);                 // swap = sirve de ida (undo) y de vuelta (redo)
        }
        // curvas que NO estaban al capturar (las creo la operacion que se esta deshaciendo): se
        // vacian y su contenido queda en el snapshot, asi el redo las devuelve enteras.
        for (size_t k = 0; k < c.size(); k++){
            if (visto[k] || c[k].c->empty()) continue;
            KFGuardada g; g.id = c[k].id; c[k].c->swap(g.kf); snap.push_back(g);
        }
        // la pose cacheada del rig que TOCAMOS (no la del que este activo) tiene que re-evaluarse
        if (lid.kind == 1 && lid.arm){ lid.arm->lastPoseFrame = -999999; lid.arm->poseDirty = true; lid.arm->poseSerial++;
                                       InvalidarSkinDeArmature(lid.arm); }
        if (ActiveAnimArm && ActiveAnimArm != lid.arm){ ActiveAnimArm->lastPoseFrame = -999999; ActiveAnimArm->poseDirty = true;
                                       ActiveAnimArm->poseSerial++; InvalidarSkinDeArmature(ActiveAnimArm); }
        if (lid.kind == 4 && lid.mesh){ lid.mesh->pose2dDirty = true; lid.mesh->lastSkinFrame = -999999; }
        // FRAMES de vertices: swap por serializacion (Deserializar libera lo vivo y reconstruye).
        // Si la malla se re-modelo (vertexSize distinto) Deserializar devuelve false y no toca nada.
        if (vfHay){ VertexAnimation* v = VfAnim();
            if (v && v->target){
                std::vector<unsigned char> now; VertexAnimSerializar(*v, now);       // lo VIVO
                if (VertexAnimDeserializar(*v, vfBlob.empty()?0:&vfBlob[0], vfBlob.size()))
                    vfBlob.swap(now);                                                 // queda con lo vivo (redo)
                extern int CurrentFrame;
                EvalVertexAnim(*v, v->target, (float)CurrentFrame); // que el viewport muestre YA la pose restaurada
                v->target->skinGeomVersion++;                       // re-subir el VBO (sino queda congelado)
            } }
    }
    // (c) lid.idx: POSICION en LA lista de su kind (ver KFLista). Es el UNICO miembro posicional:
    //     el snapshot ya no es paralelo (cada entrada lleva su KFId) y el blob de vertices va por
    //     esa misma identidad de lista.
    // (a) lid.arm / lid.mesh / lid.arm2d: punteros revalidados al resolver (ObjetoEnEscena /
    //     Arm2DEnMalla). KFId::obj se COMPARA contra los vivos, nunca se desreferencia.
    // (d) lid.kind; (b) snap[].kf y vfBlob (copias por valor, autocontenidas).
    void RemapLista(const W3dRenameDest& lista, int a, int b) {
        W3dRenameDest mia;
        if      (lid.kind == 1) mia = W3dDestClipArm(lid.arm, -1);
        else if (lid.kind == 3) mia = W3dDestCapaMalla(lid.mesh, W3dRenameDest::VertAnim, -1);
        else if (lid.kind == 4) mia = W3dDestClip2D(lid.mesh, lid.arm2d, -1);
        else                    mia = W3dDestGlobal(W3dRenameDest::SceneAnimG, -1);
        if (!W3dMismaLista(mia, lista)) return;
        if (lid.idx < 0) return;                             // el paso ya estaba muerto
        if (!RemapIndiceGuardado(lid.idx, a, b)) lid.idx = -1; // borraron ESA anim/clip/escena: no-op seguro
    }
};
static KeyframesUndo* g_pendingKeys = NULL;
void UndoKeyframesIniciar(){ delete g_pendingKeys; g_pendingKeys = new KeyframesUndo(); }
void UndoKeyframesConfirmar(){
    if (!g_pendingKeys) return;
    if (g_pendingKeys->Vacio() || !g_pendingKeys->Cambio()){ delete g_pendingKeys; g_pendingKeys = NULL; return; }
    Push(g_pendingKeys); g_pendingKeys = NULL;
}

// ============================================================================
//  EDIT MODE de HUESOS (extrude/duplicar/borrar/mover head-tail/rename): snapshot COMPLETO de bones[]
//  + boneActivo + los TRACKS de cada clip (referencian huesos por INDICE: borrar un hueso los remapea,
//  el undo tiene que devolverlos). Aplicar() = SWAP total + re-preparar el skin autorado (el rest de un
//  rig autorado se deriva de head/tail -> restaurar los huesos exige recomputar las matrices de skin).
// ============================================================================
class BonesUndo : public UndoCmd {
    Armature* a;
    std::vector<W3dBone> bones;
    int boneActivo;
    // TRACKS de los clips 3D. Antes era un vector PARALELO POSICIONAL a a->animations, con el
    // unico guard "tracks.size() == animations.size()" -> los clips son familia (2) (el "-" de la
    // tarjeta Animation borra un clip sin empujar ningun paso de undo) y eso rompia de DOS formas
    // distintas, las dos mudas:
    //   - borrar un clip de ABAJO y crear otro (el conteo vuelve a coincidir): el Ctrl+Z metia
    //     las curvas de cada clip en el clip de al lado, y las del borrado en el clip NUEVO;
    //   - borrar sin crear (el conteo NO coincide): el swap se salteaba ENTERO y el Ctrl+Z no
    //     devolvia NINGUNA curva.
    // Ahora cada entrada LLEVA SU INDICE y se remapea (RemapLista). Sin conteos: cada clip
    // recupera lo suyo, el borrado pierde su entrada (idx = -1) y un clip nuevo no recibe nada.
    struct ClipTracks { int idx; std::vector<BoneTrack> tracks; ClipTracks() : idx(-1) {} };
    std::vector<ClipTracks> tracks;
    void Capturar(){
        bones = a->bones; boneActivo = a->boneActivo;
        tracks.clear();
        tracks.resize(a->animations.size());
        for (size_t i = 0; i < a->animations.size(); i++){
            tracks[i].idx = (int)i;
            if (a->animations[i]) tracks[i].tracks = a->animations[i]->tracks;
        }
    }
public:
    BonesUndo(Armature* arm) : a(arm), boneActivo(-1) { if (a) Capturar(); }
    bool Cambio() const { // hubo cambio real? (un click que no movio nada no ensucia el stack)
        if (!a) return false;
        if (a->boneActivo != boneActivo) return true;
        if (a->bones.size() != bones.size()) return true;
        for (size_t i = 0; i < bones.size(); i++){
            const W3dBone& v = a->bones[i]; const W3dBone& s = bones[i];
            if (v.name != s.name || v.parent != s.parent || v.conectado != s.conectado) return true;
            if (v.head.x!=s.head.x || v.head.y!=s.head.y || v.head.z!=s.head.z) return true;
            if (v.tail.x!=s.tail.x || v.tail.y!=s.tail.y || v.tail.z!=s.tail.z) return true;
            if (v.select != s.select) return true;
        }
        return false;
    }
    void Aplicar(){
        if (!a) return;
        a->bones.swap(bones);
        int cur = a->boneActivo; a->boneActivo = boneActivo; boneActivo = cur;
        // cada entrada vuelve A SU CLIP (por indice remapeado), sin comparar conteos: si el
        // clip se borro la entrada esta muerta (idx = -1) y si hay clips NUEVOS no se tocan.
        for (size_t i = 0; i < tracks.size(); i++){
            const int k = tracks[i].idx;
            if (k < 0 || k >= (int)a->animations.size() || !a->animations[k]) continue;
            a->animations[k]->tracks.swap(tracks[i].tracks);
        }
        // el rest de un rig AUTORADO sale de head/tail -> recomputar matrices de skin con lo restaurado
        if (a->skinAutorado) PrepararSkinAutorado(a);
        a->poseDirty = false; a->lastPoseFrame = -999999; a->poseSerial++;
        InvalidarSkinDeArmature(a); // cache de skin/vertex-anim de las mallas del rig: stale
    }
    // (c) tracks[].idx: POSICION en Armature::animations (clips 3D), familia (2) -> se remapea.
    //     Cubre las DOS operaciones: borrar un clip (UndoBorrarClipArm) y reordenarlos
    //     (UndoMoverClipArm, que manda el swap a<->b).
    // (b) bones: snapshot COMPLETO de Armature::bones (por eso esa lista es familia (1)).
    // (c) boneActivo: posicion en bones, pero vuelve JUNTO con el snapshot completo de arriba.
    // (a) a: Armature*.
    void RemapLista(const W3dRenameDest& lista, int x, int y) {
        if (!W3dMismaLista(W3dDestClipArm(a, -1), lista)) return;
        for (size_t i = 0; i < tracks.size(); i++)
            if (!RemapIndiceGuardado(tracks[i].idx, x, y)) tracks[i].idx = -1; // borraron ESE clip
    }
};
static BonesUndo* g_pendingBones = NULL;
void UndoBonesCapturar(Armature* a){ if (a) Push(new BonesUndo(a)); }
void UndoBonesIniciar(Armature* a){ delete g_pendingBones; g_pendingBones = a ? new BonesUndo(a) : NULL; }
void UndoBonesConfirmar(){
    if (!g_pendingBones) return;
    if (!g_pendingBones->Cambio()){ delete g_pendingBones; g_pendingBones = NULL; return; }
    Push(g_pendingBones); g_pendingBones = NULL;
}
void UndoBonesCancelar(){ delete g_pendingBones; g_pendingBones = NULL; }

static PoseUndo* g_pendingPose = NULL;
void UndoPoseIniciar(Armature* a){ delete g_pendingPose; g_pendingPose = a ? new PoseUndo(a) : NULL; }
void UndoPoseConfirmar(){
    if (!g_pendingPose) return;
    if (g_pendingPose->Vacio() || !g_pendingPose->Cambio()){ delete g_pendingPose; g_pendingPose = NULL; return; }
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
class PesosUndo : public UndoCmd {
    Mesh* m;
    std::vector<VertexGroup> grupos;   // copia por VALOR (nombre + verts/pesos por control-point)
    std::vector<UVGroup>     uvgrupos; // copia por VALOR (nombre + verts/pesos por corner)
    int grupoActivo, uvGrupoActivo;
public:
    PesosUndo(Mesh* M) : m(M), grupoActivo(0), uvGrupoActivo(-1) {
        if (!m) return;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) grupos.push_back(*m->vertexGroups[i]);
        for (size_t i = 0; i < m->uvGroups.size(); i++)     uvgrupos.push_back(*m->uvGroups[i]);
        grupoActivo = m->grupoActivo; uvGrupoActivo = m->uvGrupoActivo;
    }
    bool Cambio() const { // hubo cambio real? (un click que no pinto nada no ensucia el stack)
        if (!m) return false;
        if ((int)grupos.size() != (int)m->vertexGroups.size()) return true;
        if ((int)uvgrupos.size() != (int)m->uvGroups.size()) return true;
        if (grupoActivo != m->grupoActivo || uvGrupoActivo != m->uvGrupoActivo) return true;
        for (size_t g = 0; g < grupos.size(); g++) {
            const VertexGroup* vg = m->vertexGroups[g];
            if (vg->nombre != grupos[g].nombre) return true;
            if (vg->verts != grupos[g].verts || vg->pesos != grupos[g].pesos) return true;
        }
        // UV groups (pincel del editor UV): sin esto el trazo del UV se descartaba como "sin
        // cambios" (los vertex groups no los toca)
        for (size_t g = 0; g < uvgrupos.size(); g++) {
            const UVGroup* ug = m->uvGroups[g];
            if (ug->nombre != uvgrupos[g].nombre) return true;
            if (ug->verts != uvgrupos[g].verts || ug->pesos != uvgrupos[g].pesos) return true;
        }
        return false;
    }
    void Aplicar() {
        if (!m) return;
        std::vector<VertexGroup> cur; // estado VIVO (para rehacer)
        std::vector<UVGroup>     curUV;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) cur.push_back(*m->vertexGroups[i]);
        for (size_t i = 0; i < m->uvGroups.size(); i++)     curUV.push_back(*m->uvGroups[i]);
        int curAct = m->grupoActivo, curActUV = m->uvGrupoActivo;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) delete m->vertexGroups[i];
        m->vertexGroups.clear();
        for (size_t i = 0; i < grupos.size(); i++) m->vertexGroups.push_back(new VertexGroup(grupos[i]));
        for (size_t i = 0; i < m->uvGroups.size(); i++) delete m->uvGroups[i];
        m->uvGroups.clear();
        for (size_t i = 0; i < uvgrupos.size(); i++) m->uvGroups.push_back(new UVGroup(uvgrupos[i]));
        m->grupoActivo = grupoActivo; m->uvGrupoActivo = uvGrupoActivo;
        grupos.swap(cur); uvgrupos.swap(curUV);
        grupoActivo = curAct; uvGrupoActivo = curActUV;
        m->skinGeomVersion++;        // el CSR de skinning no hashea los pesos -> invalidar
        m->lastSkinFrame = -999999;  // re-skin con los pesos restaurados
        // SKINNING 2D: los pesos por corner tambien deforman los UV -> con una POSE REAL hay que
        // re-aplicarla para que el undo se vea al toque. Con la pose en identidad NO se toca uv[]
        // (el skinning no hace nada ahi y re-aplicar solo puede pisar la edicion del usuario).
        if (m->TieneArm2D() && !m->Armature2DPoseIdentidad()) m->Armature2DAplicar();
    }
    // grupos/uvgrupos: (b) snapshot COMPLETO de Mesh::vertexGroups y Mesh::uvGroups -Aplicar las
    // reemplaza enteras-, asi que grupoActivo/uvGrupoActivo (posiciones) vuelven coherentes CON
    // la lista que restauran. Las dos son familia (1). m: (a) Mesh*.
    W3D_UNDO_SIN_INDICES
};
static PesosUndo* g_pendingPesos = NULL;
void UndoPesosIniciar(Mesh* m){ delete g_pendingPesos; g_pendingPesos = m ? new PesosUndo(m) : NULL; }
void UndoPesosConfirmar(){
    if (!g_pendingPesos) return;
    if (!g_pendingPesos->Cambio()){ delete g_pendingPesos; g_pendingPesos = NULL; return; }
    Push(g_pendingPesos); g_pendingPesos = NULL;
}

// ============================================================================
//  TRANSFORM DE UVs (G/R/S del editor UV / tarjeta "Transform UV" del panel).
//  DECISION liviano vs completo (pedido del dueno, documentada):
//   - LIVIANO (UVMapUndo): mover/rotar/escalar UVs de verts o bordes (y de caras SIN split)
//     solo cambia VALORES dentro de mesh->uv -> alcanza con snapshotear la capa uv completa
//     (patron EditMoveUndo: swap con guard de tamano). Viaja tambien uv2dRest (el rest del
//     skinning 2D acompana a la capa: editar UVs a mano lo invalida y se recaptura lazy).
//   - COMPLETO (MeshGeoUndo): el transform de CARA puede hacer SPLIT de esquinas
//     (UVSepararCarasSel duplica render-verts -> cambia vertexSize, faces3d, posRep,
//     vertCtrlPoint, vertex anims...). El snapshot liviano no puede deshacer eso, asi que
//     el caller pide IniciarCompleto ANTES del split y se reusa MeshGeoUndo tal cual.
//  Pendiente hasta confirmar: cancelar (o confirmar sin cambio real) descarta y no ensucia
//  el stack. 'cambio' lo decide el caller comparando los uv vivos contra la base del
//  transform: un split SIN mover no pushea (los duplicados identicos se re-mergean solos
//  en el proximo GenerarRender, ver UVSepararCarasSel).
// ============================================================================
class UVMapUndo : public UndoCmd {
    Mesh* m;
    std::vector<GLfloat> uv;      // capa mesh->uv completa (2 floats por render-vert)
    std::vector<GLfloat> rest2d;  // uv2dRest (rest del skinning 2D): acompana a la capa
public:
    UVMapUndo(Mesh* M) : m(M) {
        if (m && m->uv && m->vertexSize > 0) uv.assign(m->uv, m->uv + m->vertexSize * 2);
        if (m) rest2d = m->uv2dRest;
    }
    bool Vacio() const { return uv.empty(); }
    void Aplicar() {
        if (!m || !m->uv) return;
        if ((int)uv.size() != m->vertexSize * 2) return; // la topologia cambio: no tocar (robusto)
        for (size_t i = 0; i < uv.size(); i++) { GLfloat c = m->uv[i]; m->uv[i] = uv[i]; uv[i] = c; }
        rest2d.swap(m->uv2dRest);  // puede quedar vacio: se recaptura lazy al posar
        m->skinGeomVersion++;      // re-subir el VBO de uv (sino el viewport muestra el mapeo viejo)
    }
    W3D_UNDO_SIN_INDICES // uv/rest2d: (b) snapshot completo de los arrays (con guard de tamano); m: (a)
};
static UVMapUndo*   g_pendingUV    = NULL; // transform de UV liviano en curso
static MeshGeoUndo* g_pendingUVGeo = NULL; // transform de cara CON split en curso (snapshot total)

void UndoUVIniciar(Mesh* m) {
    delete g_pendingUV;    g_pendingUV = NULL;
    delete g_pendingUVGeo; g_pendingUVGeo = NULL;
    if (m) g_pendingUV = new UVMapUndo(m);
}
void UndoUVIniciarCompleto(Mesh* m) { // llamar ANTES de UVSepararCarasSel (snapshot pre-split)
    delete g_pendingUV;    g_pendingUV = NULL;
    delete g_pendingUVGeo; g_pendingUVGeo = NULL;
    if (m) g_pendingUVGeo = new MeshGeoUndo(m);
}
void UndoUVCancelar() {
    delete g_pendingUV;    g_pendingUV = NULL;
    delete g_pendingUVGeo; g_pendingUVGeo = NULL;
}
void UndoUVConfirmar(bool cambio) {
    if (cambio && g_pendingUVGeo)                          { Push(g_pendingUVGeo); g_pendingUVGeo = NULL; }
    else if (cambio && g_pendingUV && !g_pendingUV->Vacio()){ Push(g_pendingUV);    g_pendingUV = NULL; }
    UndoUVCancelar(); // descarta lo que haya quedado pendiente (sin cambio real)
}

// ============================================================================
//  ARMATURE 2D del mesh (huesos 2D del editor UV): patron BonesUndo adaptado a W3dBone2D.
//  Snapshot COMPLETO de los huesos del armature 2D ACTIVO (con su hueso activo) + la capa
//  uv y uv2dRest: la POSE 2D
//  deforma mesh->uv (Armature2DAplicar) y borrar huesos re-aplica el skinning, asi que la
//  capa vuelve byte-exacta con el swap (sin recomputar nada). Los VERTEX GROUPS no se
//  snapshotean (ver nota en Undo.h): Bone2DBorrar los conserva a proposito y un grupo de
//  mas tras deshacer un Add es inofensivo.
//  Los TRACKS de los clips 2D SI van: borrar/reordenar huesos remapea Bone2DTrack::bone y
//  se lleva puestas las curvas del hueso borrado -> sin snapshot, el undo devolvia los
//  huesos pero las curvas ya no volvian. Se guardan solo los tracks (el clip -nombre/fps/
//  rango- no lo tocan estas operaciones), en paralelo a los clips de ESE armature.
// ============================================================================
class Bones2DUndo : public UndoCmd {
    Mesh* m;
    Armature2D* arm;              // el armature 2D CONCRETO que se snapshoteo (no "el activo": el
                                  // usuario puede cambiar de armature entre capturar y deshacer)
    std::vector<W3dBone2D> bones;
    int activo;
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
    Bones2DUndo(Mesh* M) : m(M), arm(NULL), activo(-1) {
        if (!m) return;
        arm = m->Arm2DActivoP();
        if (arm) { bones = arm->huesos; activo = arm->boneActivo; }
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
    bool Cambio() const { // hubo cambio real? (un G+Esc / click sin mover no ensucia el stack)
        if (!Vive()) return false;
        if (arm->boneActivo != activo) return true;
        if (arm->huesos.size() != bones.size()) return true;
        for (size_t i = 0; i < bones.size(); i++) {
            const W3dBone2D& v = arm->huesos[i]; const W3dBone2D& s = bones[i];
            if (v.nombre != s.nombre || v.padre != s.padre || v.select != s.select) return true;
            if (v.conectado != s.conectado || v.selHead != s.selHead || v.selTail != s.selTail) return true;
            if (v.headU != s.headU || v.headV != s.headV || v.tailU != s.tailU || v.tailV != s.tailV) return true;
            if (v.poseTU != s.poseTU || v.poseTV != s.poseTV || v.poseRot != s.poseRot ||
                v.poseSX != s.poseSX || v.poseSY != s.poseSY) return true;
        }
        return false;
    }
    void Aplicar() {
        if (!Vive()) return;
        arm->huesos.swap(bones);
        int c = arm->boneActivo; arm->boneActivo = activo; activo = c;
        if (arm->boneActivo >= (int)arm->huesos.size()) arm->boneActivo = -1; // guard
        // la capa uv vuelve TAL CUAL estaba (swap byte-exacto, con guard de tamano)
        if (m->uv && (int)uv.size() == m->vertexSize * 2)
            for (size_t i = 0; i < uv.size(); i++) { GLfloat t = m->uv[i]; m->uv[i] = uv[i]; uv[i] = t; }
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
    // (c) activo: posicion en arm->huesos, vuelve JUNTO con el snapshot completo de los huesos.
    // (a) m: Mesh*; arm: Armature2D* VALIDADO al aplicar (Vive()).
    void RemapLista(const W3dRenameDest& lista, int x, int y) {
        if (!W3dMismaLista(W3dDestClip2D(m, arm, -1), lista)) return;
        for (size_t i = 0; i < tracks2d.size(); i++)
            if (!RemapIndiceGuardado(tracks2d[i].idx, x, y)) tracks2d[i].idx = -1; // borraron ESE clip
    }
};
static Bones2DUndo* g_pendingB2D = NULL;
void UndoBones2DCapturar(Mesh* m){ if (m) Push(new Bones2DUndo(m)); } // op discreta: push directo (el caller ya valido que va a mutar)
void UndoBones2DIniciar(Mesh* m){ delete g_pendingB2D; g_pendingB2D = m ? new Bones2DUndo(m) : NULL; }
void UndoBones2DConfirmar(){
    if (!g_pendingB2D) return;
    if (!g_pendingB2D->Cambio()){ delete g_pendingB2D; g_pendingB2D = NULL; return; }
    Push(g_pendingB2D); g_pendingB2D = NULL;
}
void UndoBones2DCancelar(){ delete g_pendingB2D; g_pendingB2D = NULL; }

// RENAME de un hueso 2D: comando ATOMICO (ver Undo.h) = snapshot de los huesos (que ya guarda el
// nombre viejo del hueso) + un RenameUndo por cada UV group homonimo. Un solo Ctrl+Z devuelve las
// dos puntas del binding a la vez.
class Bone2DRenameUndo : public ContenedorUndo {
    Bones2DUndo* huesos;
    std::vector<RenameUndo*> grupos;
protected:
    // contenedor: PROPAGA a las dos partes (lo hace ContenedorUndo). El 'huesos' tambien tiene
    // que recibirlo: desde que Bones2DUndo lleva los tracks de los clips 2D por indice,
    // saltearlo dejaba ESE snapshot stale (el mismo bug, una capa mas adentro).
    // Aplicar: primero los huesos, despues los nombres de grupo (toggles independientes).
    void Partes(std::vector<UndoCmd*>& out){
        out.push_back(huesos);
        for (size_t i=0;i<grupos.size();i++) out.push_back(grupos[i]);
    }
public:
    Bone2DRenameUndo(Bones2DUndo* h) : huesos(h) {}
    ~Bone2DRenameUndo(){ delete huesos; for (size_t i=0;i<grupos.size();i++) delete grupos[i]; }
    void Agregar(RenameUndo* r){ if (r) grupos.push_back(r); }
};
void UndoBone2DRenameCapturar(Mesh* m, const std::vector<W3dRenameDest>& grupos){
    if (!m) return;
    Bone2DRenameUndo* cmd = new Bone2DRenameUndo(new Bones2DUndo(m));
    // (malla, lista UVGroup, indice): con un std::string* el destino quedaba 'Directo' y el
    // resolver devolvia NULL para siempre -> el undo devolvia el hueso y NO el UV group
    for (size_t i = 0; i < grupos.size(); i++)
        if (W3dDestResolver(grupos[i])) cmd->Agregar(new RenameUndo(grupos[i]));
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
//  Los UV GROUPS del rig NO se tocan al borrar (Mesh::Arm2DBorrar los deja): son los PESOS por
//  corner y se conservan a proposito, igual que Bone2DBorrar conserva los vertex groups. Asi el
//  undo devuelve el rig ya bindeado (el binding es por NOMBRE hueso<->UV group) sin snapshotear
//  nada; un UV group sin hueso es inofensivo (no deforma).
// ============================================================================
class Arm2DListaUndo : public UndoCmd {
    Mesh* m;
    Armature2D* arm;   // el armature concreto que se agrego/borro
    int idx;           // su posicion en la lista
    int activo;        // Mesh::armature2dActivo cuando el armature estaba DENTRO
    bool dentro;       // esta AHORA en la malla? (false = lo tenemos nosotros)
    int Indice() const {
        if (!m || !arm) return -1;
        for (size_t i = 0; i < m->armatures2d.size(); i++) if (m->armatures2d[i] == arm) return (int)i;
        return -1;
    }
    void Refrescar() { // el skinning cambio (un rig entro o salio): re-aplicar y re-subir el VBO
        if (!m) return;
        m->Armature2DRestCapturar();
        m->Armature2DAplicar();
        m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true;
        m->skinGeomVersion++;
        // sacar/devolver un armature REACOMODA el activo (aritmetica sobre el indice) -> el clip
        // que muestra el timeline en kind 4 puede ser OTRO: hay que recargar su rango (sino el
        // Start/End que quedan en pantalla son de un clip ajeno y editarlos lo escribe ahi).
        Arm2DSincronizarRango(m);
    }
public:
    Arm2DListaUndo(Mesh* M, Armature2D* A, int I, int Act, bool Dentro)
        : m(M), arm(A), idx(I), activo(Act), dentro(Dentro) {}
    ~Arm2DListaUndo() { if (!dentro) delete arm; } // quedo afuera de la malla: lo liberamos aca
    void Aplicar() {
        if (!m || !arm) return;
        if (dentro) {                       // SACAR el armature de la malla
            int i = Indice(); if (i < 0) return;
            idx = i; activo = m->armature2dActivo;
            m->armatures2d.erase(m->armatures2d.begin() + i);
            if (m->armature2dActivo > i) m->armature2dActivo--;
            if (m->armature2dActivo >= (int)m->armatures2d.size())
                m->armature2dActivo = (int)m->armatures2d.size() - 1;
            DopeRemapIndiceClave("arm2d:" + DopeIdDueno(m) + "/a", i, -1); // la seleccion del dope indexa esta lista
            dentro = false;
        } else {                            // DEVOLVERLO a su posicion original
            int i = idx;
            if (i < 0) i = 0;
            if (i > (int)m->armatures2d.size()) i = (int)m->armatures2d.size();
            m->armatures2d.insert(m->armatures2d.begin() + i, arm);
            DopeInsertarIndiceClave("arm2d:" + DopeIdDueno(m) + "/a", i);  // ...y en la vuelta, al reves
            m->armature2dActivo = activo;
            if (m->armature2dActivo < 0 || m->armature2dActivo >= (int)m->armatures2d.size())
                m->armature2dActivo = i;
            dentro = true;
        }
        Refrescar();
    }
    // idx/activo son (c): POSICIONES en Mesh::armatures2d. Esa lista es familia (1): sacar o
    // devolver un armature 2D SIEMPRE pasa por UndoArm2DAgregado/UndoArm2DBorrar, que empujan
    // ESTE mismo comando (nadie la toca "por su cuenta"), y ademas mientras el armature esta
    // DENTRO la posicion se re-resuelve POR PUNTERO (Indice()) y al devolverlo se clampea.
    W3D_UNDO_SIN_INDICES // arm: (a) Armature2D* (identidad); m: (a) Mesh*
};
// el armature YA se agrego (idx = su posicion): deshacer lo SACA de la lista sin destruirlo.
void UndoArm2DAgregado(Mesh* m, int idx) {
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return;
    Push(new Arm2DListaUndo(m, m->armatures2d[idx], idx, m->armature2dActivo, true));
}
// BORRA el armature idx guardandolo en el paso de undo (no lo destruye: lo adopta el comando).
bool UndoArm2DBorrar(Mesh* m, int idx) {
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return false;
    Armature2D* a = m->armatures2d[idx];
    const int actAntes = m->armature2dActivo;
    m->armatures2d.erase(m->armatures2d.begin() + idx);
    if (m->armature2dActivo > idx) m->armature2dActivo--;
    if (m->armature2dActivo >= (int)m->armatures2d.size())
        m->armature2dActivo = (int)m->armatures2d.size() - 1;
    DopeRemapIndiceClave("arm2d:" + DopeIdDueno(m) + "/a", idx, -1); // la seleccion del dope indexa esta lista
    Push(new Arm2DListaUndo(m, a, idx, actAntes, false));
    m->Armature2DRestCapturar();
    m->Armature2DAplicar();                 // el rig borrado ya no deforma
    m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true;
    m->skinGeomVersion++;
    Arm2DSincronizarRango(m);               // el activo cambio: el timeline (kind 4) muestra otro clip
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
//      igual que DeleteUndo adopta los objetos borrados y Arm2DListaUndo el armature 2D.
//
//  PATRON: SWAP del stack completo (como MeshGeoUndo). Se guarda la lista ENTERA con su ORDEN
//  -no "el que se quito y donde"- asi el mismo comando cubre las tres operaciones y ademas no
//  guarda ninguna POSICION que despues se corra (el "donde" de un Remove se lo comia el
//  reordenar de los botones subir/bajar, que tampoco dejaba paso de undo).
//
//  DUENO DE LA MEMORIA: la malla es la duena de los modificadores que TIENE; este comando es
//  el dueno de los que quedaron en su snapshot y ya NO estan en la malla ('propios'). Esa
//  cuenta se rehace en cada Aplicar (con la malla viva) y NO se puede rehacer despues: cuando
//  ~Object avisa, ~Mesh ya corrio LiberarModificadores y la lista de la malla esta vacia (ver
//  el bloque de ganchos en Objects.h). Por eso 'propios' se mantiene EAGER.
// ============================================================================
class ModStackUndo : public UndoCmd {
    Mesh* m;                            // (a) Mesh*; se suelta si lo destruyen (DesvincularDetachados)
    std::vector<Modifier*> stack;       // (b) snapshot COMPLETO de Mesh::modificadores, CON su orden
    std::vector<Modifier*> propios;     // (a) los del snapshot que ya no estan en la malla: los libera el dtor
    int activo;                         // (c)->(b): Mesh::modificadorActivo, vuelve JUNTO con la lista
    // recalcula que modificadores del snapshot NO estan en la malla (esos son nuestros).
    // Solo vale llamarla con 'm' vivo.
    void RecalcularPropios() {
        propios.clear();
        if (!m) return;
        for (size_t i = 0; i < stack.size(); i++) {
            bool enMalla = false;
            for (size_t k = 0; k < m->modificadores.size() && !enMalla; k++)
                if (m->modificadores[k] == stack[i]) enMalla = true;
            if (!enMalla) propios.push_back(stack[i]);
        }
    }
public:
    ModStackUndo(Mesh* M) : m(M), stack(M->modificadores), activo(M->modificadorActivo) {}
    // el snapshot se toma ANTES de la operacion; esto se llama DESPUES, con la malla ya
    // cambiada, para saber a quien adoptamos. Va junto en la misma puerta (UndoModAgregar y
    // companiia) para que no se pueda olvidar.
    void Sincronizar() { RecalcularPropios(); }
    ~ModStackUndo() { for (size_t i = 0; i < propios.size(); i++) delete propios[i]; }
    void Aplicar() {
        if (!m) return;
        std::vector<Modifier*> tmp = m->modificadores;
        m->modificadores = stack; stack = tmp;
        const int act = m->modificadorActivo; m->modificadorActivo = activo; activo = act;
        if (m->modificadorActivo >= (int)m->modificadores.size())
            m->modificadorActivo = (int)m->modificadores.size() - 1;   // -1 si quedo vacio
        RecalcularPropios();       // la duena de cada modificador cambio: rehacer la cuenta
        // el stack cambio -> rehacer el preview. Con el stack VACIO tambien hay que llamarla:
        // deja genValido=false y el render vuelve a la malla editable (sino se queda dibujando
        // el resultado del modificador que el Ctrl+Z acaba de sacar).
        m->GenerarMallaModificada();
    }
    // la MALLA se esta destruyendo: ~Mesh ya libero los modificadores que ella tenia, pero los
    // NUESTROS no los toco nadie (no estaban en su lista). Se liberan aca y se suelta todo: sin
    // esto quedaban colgados los punteros de 'stack' Y se filtraban los adoptados.
    void DesvincularDetachados(Object* borrado) {
        if (borrado != (Object*)m) return;
        for (size_t i = 0; i < propios.size(); i++) delete propios[i];
        propios.clear(); stack.clear(); m = NULL;
    }
    // 'activo' es (c) contra Mesh::modificadores, pero vuelve JUNTO con el snapshot COMPLETO de
    // esa lista (b) -> queda coherente con la lista que restaura, y se clampea. La lista no
    // tiene destinos de rename por indice (no hay W3dRenameDest de modificador) ni pasa por
    // RemapEnStacks: si algun dia se puede renombrar un modificador, esto necesita remapeo.
    W3D_UNDO_SIN_INDICES
};
// LA PUERTA UNICA del usuario al stack de modificadores (ver Undo.h). Las tres capturan,
// operan y sincronizan la adopcion en el MISMO lugar: no hay forma de hacer la mitad.
static void ModPushSinc(ModStackUndo* c) { if (c) { c->Sincronizar(); Push(c); } }
void UndoModAgregar(Mesh* m, int tipo) {
    if (!m) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->AgregarModificador(tipo);
    ModPushSinc(c);
}
void UndoModQuitar(Mesh* m) {
    if (!m || m->modificadorActivo < 0 || m->modificadorActivo >= (int)m->modificadores.size()) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->SacarModificadorActivo();   // NO lo libera: queda en el snapshot de 'c', que lo adopta
    ModPushSinc(c);
}
// VACIA el stack entero sin liberar nada (los adopta el paso de undo). Lo usa el "Apply" de
// Screw/Subdivision, que hornea TODO el stack de una: antes hacia 'delete' en un for y era el
// mismo agujero que el boton Remove (trabajo perdido + direcciones libres para reciclar con un
// DeleteUndo guardandose esos punteros).
void UndoModVaciar(Mesh* m) {
    if (!m || m->modificadores.empty()) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->modificadores.clear(); m->modificadorActivo = -1;
    ModPushSinc(c);
}
void UndoModMover(Mesh* m, int dir) {
    if (!m) return;
    const int i = m->modificadorActivo, j = i + dir;
    if (i < 0 || i >= (int)m->modificadores.size() || j < 0 || j >= (int)m->modificadores.size()) return;
    ModStackUndo* c = new ModStackUndo(m);
    m->MoverModificador(dir);
    ModPushSinc(c);
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
//      un bug: es que asi este comando NO TIENE NINGUN MIEMBRO (c). 'modificadorActivo' guardado
//      crudo ES un indice contra una lista viva, y la tabla de Undo.h tiene que explicar por que
//      se salva; un serial es IDENTIDAD y el W3D_UNDO_SIN_INDICES de abajo pasa a ser cierto en
//      vez de una excepcion argumentada. Ademas es la MISMA identidad con la que ya lo nombran
//      la puerta de refs (RefEntry::conSerial) y ConstraintPorSerial: una sola forma de decir
//      "este constraint", en vez de dos que hay que mantener de acuerdo.
//
//  (2) LOS DETACHADOS SUELTAN SU FUENTE. Un constraint que ya no esta en el objeto no lo
//      enumera Object::RefsObjeto, asi que cuando su objeto FUENTE se destruye no hay nadie que
//      le ponga el puntero en NULL: quedaria colgado y la direccion se recicla (la familia
//      'modtargetuaf'). Se limpia en DesvincularDetachados, que es justo el aviso de ~Object.
//      OJO: esto es DEFENSA, no algo que un test pueda mirar sin ASAN -- para observarlo habria
//      que liberar el objeto fuente conservando vivo este comando, y lo unico que libera al
//      fuente (que el DeleteUndo se caiga del historial) se lleva puesto tambien a este.
// ============================================================================
class ConStackUndo : public UndoCmd {
    Object* o;                            // (a) Object*; se suelta si lo destruyen (DesvincularDetachados)
    std::vector<W3dConstraint*> stack;    // (b) snapshot COMPLETO de Object::constraints, CON su orden
    std::vector<W3dConstraint*> propios;  // (a) los del snapshot que ya no estan en el objeto: los libera el dtor
    unsigned int activoSerial;            // IDENTIDAD del elegido en la lista (0 = ninguno). Ver (1) arriba.
    static unsigned int SerialActivo(const Object* ob) {
        if (!ob || ob->constraintActivo < 0 || ob->constraintActivo >= (int)ob->constraints.size()) return 0;
        return ob->constraints[ob->constraintActivo]->serial.v;
    }
    static int IndicePorSerial(const Object* ob, unsigned int s) {
        if (!ob || !s) return -1;
        for (size_t i = 0; i < ob->constraints.size(); i++)
            if (ob->constraints[i]->serial.v == s) return (int)i;
        return -1;   // ya no esta (ej: el Ctrl+Z de un Add): "ninguno elegido", que es la verdad
    }
    void RecalcularPropios() {
        propios.clear();
        if (!o) return;
        for (size_t i = 0; i < stack.size(); i++) {
            bool enObjeto = false;
            for (size_t k = 0; k < o->constraints.size() && !enObjeto; k++)
                if (o->constraints[k] == stack[i]) enObjeto = true;
            if (!enObjeto) propios.push_back(stack[i]);
        }
    }
public:
    ConStackUndo(Object* O) : o(O), stack(O->constraints), activoSerial(SerialActivo(O)) {}
    void Sincronizar() { RecalcularPropios(); }
    ~ConStackUndo() { for (size_t i = 0; i < propios.size(); i++) delete propios[i]; }
    void Aplicar() {
        if (!o) return;
        const unsigned int actAhora = SerialActivo(o);   // el estado NUEVO, para el redo
        std::vector<W3dConstraint*> tmp = o->constraints;
        o->constraints = stack; stack = tmp;
        o->SetConstraintActivo(IndicePorSerial(o, activoSerial));
        activoSerial = actAhora;
        RecalcularPropios();       // el dueno de cada constraint cambio: rehacer la cuenta
        // (no hay nada que regenerar: el stack se evalua AL DIBUJAR, no hornea geometria; el
        //  redibujado lo pide el que llamo al Ctrl+Z, como con cualquier otro paso)
    }
    // el OBJETO se esta destruyendo: ~Object ya libero los constraints que el tenia, pero los
    // NUESTROS no los toco nadie (no estaban en su lista). Y si el que se destruye es un objeto
    // FUENTE, los detachados son los unicos punteros a el que la puerta de refs no enumera.
    void DesvincularDetachados(Object* borrado) {
        for (size_t i = 0; i < propios.size(); i++)
            if (propios[i] && propios[i]->fuenteObj == borrado) propios[i]->fuenteObj = NULL;
        if (borrado != o) return;
        for (size_t i = 0; i < propios.size(); i++) delete propios[i];
        propios.clear(); stack.clear(); o = NULL;
    }
    // no guarda NINGUN indice: el activo va por serial (ver arriba) y la lista no tiene destinos
    // de rename por indice (el nombre de un constraint se renombra por PUNTERO, W3dDestNombre).
    W3D_UNDO_SIN_INDICES
};
// ---- las operaciones CRUDAS del stack. A diferencia de las del Mesh viven aca adentro y son
// static a proposito: los constraints no los agrega/saca/mueve ningun otro camino (la carga del
// .w3d hace push_back directo del que leyo), asi que no hay forma de saltearse el paso de undo.
static bool ConNombreExiste(const std::string& n, void* ctx) {
    const Object* ob = (const Object*)ctx;
    for (size_t i = 0; i < ob->constraints.size(); i++)
        if (ob->constraints[i] && ob->constraints[i]->nombre == n) return true;
    return false;
}
static void ConPushSinc(ConStackUndo* c) { if (c) { c->Sincronizar(); Push(c); } }

void UndoConAgregar(Object* o, int tipo) {
    if (!o) return;
    ConStackUndo* c = new ConStackUndo(o);
    // el nombre arranca en el del TIPO y se uniquifica en el espacio del OBJETO (dos billboards
    // en el mismo objeto -> "Billboard" y "Billboard.001"; en otro objeto vuelve a haber un
    // "Billboard" y no se pisan). Es el mismo espacio que declara W3dNombresJuntarEspacios.
    W3dConstraint* nc = new W3dConstraint(tipo);
    nc->nombre = W3dNombreUnico(W3dNombreTipoConstraint(tipo), "Constraint", ConNombreExiste, o);
    o->constraints.push_back(nc);
    o->constraintActivo = (int)o->constraints.size() - 1;   // el nuevo queda elegido
    ConPushSinc(c);
}
void UndoConQuitar(Object* o) {
    if (!o || o->constraintActivo < 0 || o->constraintActivo >= (int)o->constraints.size()) return;
    ConStackUndo* c = new ConStackUndo(o);
    // NO se libera: queda en el snapshot de 'c', que lo adopta (si el boton hiciera delete, la
    // direccion quedaria libre para reciclar con un RefEntry todavia guardandose ese serial)
    o->constraints.erase(o->constraints.begin() + o->constraintActivo);
    if (o->constraintActivo >= (int)o->constraints.size())
        o->constraintActivo = (int)o->constraints.size() - 1;   // -1 si quedo vacio
    ConPushSinc(c);
}
void UndoConMover(Object* o, int dir) {   // -1 = sube (hacia el principio), +1 = baja
    if (!o) return;
    const int i = o->constraintActivo, j = i + dir;
    if (i < 0 || i >= (int)o->constraints.size() || j < 0 || j >= (int)o->constraints.size()) return;
    ConStackUndo* c = new ConStackUndo(o);
    W3dConstraint* tmp = o->constraints[i]; o->constraints[i] = o->constraints[j]; o->constraints[j] = tmp;
    o->constraintActivo = j;   // el elegido sigue al constraint MOVIDO
    ConPushSinc(c);
}

void UndoDeshacer() {
    if (g_undo.empty()) return;
    UndoCmd* c = g_undo.back(); g_undo.pop_back();
    c->Aplicar();          // intercambia: el comando queda con el estado NUEVO
    g_redo.push_back(c);   // disponible para rehacer
}
void UndoRehacer() {
    if (g_redo.empty()) return;
    UndoCmd* c = g_redo.back(); g_redo.pop_back();
    c->Aplicar();          // intercambia de nuevo: re-aplica el cambio
    g_undo.push_back(c);
}
void UndoLimpiar() {
    BorrarCmds(g_undo);   // saca del stack y despues libera (ver BorrarCmds)
    LimpiarRedo();
    g_grupos.clear();     // las bases de los grupos abiertos indexan un stack que ya no existe
    if (g_pendingRep){ delete g_pendingRep; g_pendingRep = NULL; } // mudanza a medio hacer
    if (g_pendingT)  { delete g_pendingT;  g_pendingT  = NULL; }
    if (g_pendingEM) { delete g_pendingEM; g_pendingEM = NULL; }
    if (g_pendingMat){ delete g_pendingMat; g_pendingMat = NULL; }
    // Ningun pendiente puede sobrevivir a un reset de escena. PoseUndo se queda con un Armature* crudo: si la
    // escena se rehace con un transform de pose a medio hacer, ese puntero queda colgado y el proximo
    // UndoPoseConfirmar lo desreferencia.
    if (g_pendingKeys){ delete g_pendingKeys; g_pendingKeys = NULL; }
    if (g_pendingPose){ delete g_pendingPose; g_pendingPose = NULL; }
    if (g_pendingPesos){ delete g_pendingPesos; g_pendingPesos = NULL; } // trazo de weight paint a medio hacer
    if (g_pendingBones){ delete g_pendingBones; g_pendingBones = NULL; } // drag de head/tail a medio hacer
    UndoUVCancelar();      // transform de UV a medio hacer (liviano o con split)
    UndoBones2DCancelar(); // transform de huesos 2D a medio hacer
}
bool UndoHayAlgo() { return !g_undo.empty(); }
bool UndoHayRedo() { return !g_redo.empty(); }

void UndoCapturarModo()                       { Push(new ModeUndo(InteractionMode)); }
void UndoCapturarRename(const W3dRenameDest& destino) { if (W3dDestResolver(destino)) Push(new RenameUndo(destino)); }
// N nombres en UN SOLO paso de undo: un rename que arrastra su contraparte por
// nombre (vertex group <-> hueso, UV group <-> hueso 2D, refs de scripts, riel de
// camara, targetName...) tiene que volver ENTERO con un Ctrl+Z. En pasos
// separados el estado intermedio deja el binding roto (ya paso con los huesos 2D,
// ver Bone2DRenameUndo). Los NULL y repetidos se descartan.
// FUNDE los ultimos 'n' comandos en UNO solo: Ctrl+Z los deshace juntos. Sirve para
// las operaciones compuestas que se arman con capturas ya existentes (ej: rename de
// hueso 3D = snapshot de bones + los nombres de los vertex groups homonimos). El orden
// de aplicacion se conserva; con n<=1 o el stack corto no hace nada.
void UndoFundirUltimos(int n) {
    if (n < 2 || (int)g_undo.size() < n) return;
    CompuestoUndo* cmd = new CompuestoUndo();
    for (int i = (int)g_undo.size() - n; i < (int)g_undo.size(); i++) cmd->Agregar(g_undo[i]);
    g_undo.erase(g_undo.end() - n, g_undo.end());
    g_undo.push_back(cmd);   // NO por Push(): fundir no es una accion nueva (el redo ya se limpio)
}

// ---- GRUPO: todo lo empujado entre Iniciar y Fin es UN SOLO Ctrl+Z (ver Undo.h) ----
void UndoGrupoIniciar() { g_grupos.push_back(g_undo.size()); }
void UndoGrupoFin() {
    if (g_grupos.empty()) return;
    size_t base = g_grupos.back(); g_grupos.pop_back();
    if (base > g_undo.size()) base = g_undo.size();   // el desalojo se comio parte del grupo
    const int n = (int)(g_undo.size() - base);
    if (n > 1) UndoFundirUltimos(n);
}

// destinos de IDENTIDAD ESTABLE (huesos 3D/2D, mesh parts, refs de lua, capas de la malla):
// ver W3dRenameDest en Undo.h. Los repetidos y los que ya no existen se descartan.
void UndoCapturarRenames(const std::vector<W3dRenameDest>& destinos) {
    if (destinos.empty()) return;
    if (destinos.size() == 1) { UndoCapturarRename(destinos[0]); return; }
    MultiRenameUndo* cmd = new MultiRenameUndo();
    for (size_t i = 0; i < destinos.size(); i++) {
        bool rep = false;
        for (size_t k = 0; k < i; k++) if (MismoDest(destinos[k], destinos[i])) { rep = true; break; }
        if (!rep) cmd->Agregar(destinos[i]);
    }
    if (cmd->Vacio()) { delete cmd; return; }
    Push(cmd);
}
void UndoCapturarSeleccion()                  { Push(new SelectUndo()); }
void UndoCapturarSeleccionEdit(Mesh* m)       { if (m) Push(new SelectEditUndo(m)); }
void UndoCapturarMaterial(Mesh* m, int idx)   { if (m) Push(new MaterialUndo(m, idx)); }
void UndoCapturarMallaGeo(Mesh* m)            { if (m) Push(new MeshGeoUndo(m)); } // snapshot ANTES del op

// BORRAR objetos: DETACHA los seleccionados (sin liberar) y guarda el comando. Reemplaza al delete real.
// Devuelve true si detacho algo (el caller no tiene que borrar nada mas).
bool UndoCapturarBorrado(bool incCol) {
    DeleteUndo* d = new DeleteUndo(incCol);
    if (d->Vacio()) { delete d; return false; }
    Push(d);
    return true;
}

// ============================================================================
//  REPARENT (ver ReparentUndo arriba): pendiente hasta confirmar.
//
//  DOS DETALLES QUE NO SON COSMETICOS:
//  (1) W3dNombresCargando SUPRIME la captura. Es LA MISMA puerta que ya apaga la
//      renumeracion del embudo (W3dAdjuntarA) y la levantan los loaders -que arman el arbol a
//      mano y no tienen que dejar historial- y el modal "mover" del outliner, que hace N
//      pasos provisorios y empuja SU PROPIO paso al confirmar (y ninguno al cancelar). El
//      contrato es: el que levanta la puerta es el dueno de empujar el paso.
//  (2) EL PASO DEL ARBOL VA PRIMERO DEL GRUPO, no ultimo. Cuando la mudanza cruza de escena,
//      el embudo empuja ADEMAS el paso de NOMBRES, y ese ya esta en el stack cuando esto
//      confirma. Si el compuesto quedara [nombres, arbol], el Ctrl+Z restauraria el nombre
//      viejo con el objeto TODAVIA en la escena destino -donde ese nombre puede estar
//      tomado-, se re-uniquificaria (W3dNombreLibrePara) y el usuario recuperaria "Btn.001"
//      en vez de "Btn". Insertandolo en la BASE del grupo, el Ctrl+Z devuelve primero el
//      arbol y despues los nombres, que es el orden que hace falta (y el redo, al aplicar en
//      el mismo orden, tambien queda bien).
// ============================================================================
void UndoReparentIniciar(Object* o) {
    // la puerta suprimida se chequea ANTES de descartar el pendiente: adentro de un modal que
    // ya capturo (el "mover" del outliner) los reparents intermedios pasan por aca, y pisarle
    // el snapshot al modal le dejaba el Confirmar sin nada que empujar (la mudanza no quedaba
    // en el historial y el stack de redo NO se vaciaba: justo el agujero del fallo A).
    if (W3dNombresCargando) return;
    if (g_pendingRep) { delete g_pendingRep; g_pendingRep = NULL; }
    if (!o) return;
    g_pendingRep = new ReparentUndo(o);
}
void UndoReparentCancelar() { if (g_pendingRep) { delete g_pendingRep; g_pendingRep = NULL; } }
void UndoReparentConfirmar() {
    ReparentUndo* c = g_pendingRep;
    g_pendingRep = NULL;
    if (!c) return;
    if (!c->Difiere()) { delete c; return; }   // quedo donde estaba: no ensucia el historial
    LimpiarRedo();                             // accion NUEVA (esto es lo que cierra el fallo A)
    size_t base = g_grupos.empty() ? g_undo.size() : g_grupos.back();
    if (base > g_undo.size()) base = g_undo.size();
    g_undo.insert(g_undo.begin() + base, c);
    DesalojarViejos();
}

void UndoTransformIniciar() {
    if (g_pendingT) delete g_pendingT;
    g_pendingT = new TransformUndo();
}
void UndoTransformIniciarObj(Object* o) {
    if (!o) return;
    if (g_pendingT) delete g_pendingT;
    g_pendingT = new TransformUndo(o);
}
void UndoTransformConfirmar() {
    if (!g_pendingT) return;
    // sin cambio real (un click que armo el drag y solto) no se pushea: si no, cada
    // click sobre un elemento del Editor 2D dejaba un paso de undo que "no hacia nada"
    if (!g_pendingT->Vacio() && g_pendingT->Difiere()) Push(g_pendingT); else delete g_pendingT;
    g_pendingT = NULL;
}
void UndoTransformCancelar() {
    if (g_pendingT) { delete g_pendingT; g_pendingT = NULL; }
}

void UndoEditMoveIniciar(Mesh* m) {
    if (g_pendingEM) delete g_pendingEM;
    g_pendingEM = new EditMoveUndo(m);
}
void UndoEditMoveConfirmar() {
    if (!g_pendingEM) return;
    if (!g_pendingEM->Vacio()) Push(g_pendingEM); else delete g_pendingEM;
    g_pendingEM = NULL;
}
void UndoEditMoveCancelar() {
    if (g_pendingEM) { delete g_pendingEM; g_pendingEM = NULL; }
}

// COLOR (lo llama el ColorPicker al cerrar): pushea solo si el color cambio (cancelar restaura -> no pushea)
void UndoCapturarColor(GLfloat* target, const GLfloat* viejo) {
    if (!target || !viejo) return;
    bool cambio = false; for (int i=0;i<4;i++) if (target[i] != viejo[i]) cambio = true;
    if (cambio) Push(new ColorUndo(target, viejo));
}

// MODIFICACION de material (checkbox/shininess): pendiente -> snapshot al empezar a tocar, push al soltar.
void UndoMaterialModIniciar(Material* m) {
    if (!m) return;
    if (g_pendingMat && g_pendingMat->Mat() == m) return; // ya hay snapshot de este material
    if (g_pendingMat) delete g_pendingMat;                // otro material sin commitear -> descarta
    g_pendingMat = new MaterialModUndo(m);
}
void UndoMaterialModCommit() { // lo llama el panel cada frame al soltar el mouse
    if (!g_pendingMat) return;
    if (g_pendingMat->Difiere()) Push(g_pendingMat); else delete g_pendingMat;
    g_pendingMat = NULL;
}

// ============================================================================
//  REMAPEO DE LOS COMANDOS PENDIENTES (ver RemapEnStacks, arriba)
//  Un pendiente es un comando YA CAPTURADO -con sus indices adentro- que todavia no entro a
//  ningun stack. Se le pasa el mismo aviso que a los stacks; su propio RemapLista decide.
//  AL AGREGAR UN PENDIENTE NUEVO, SUMARLO ACA (es la lista completa de los g_pending*).
// ============================================================================
static void RemapEnPendientes(const W3dRenameDest& lista, int a, int b) {
    if (g_pendingRep)  g_pendingRep->RemapLista(lista, a, b);   // ReparentUndo: sin indices de lista
    if (g_pendingT)    g_pendingT->RemapLista(lista, a, b);
    if (g_pendingEM)   g_pendingEM->RemapLista(lista, a, b);
    if (g_pendingMat)  g_pendingMat->RemapLista(lista, a, b);
    if (g_pendingJoin) g_pendingJoin->RemapLista(lista, a, b);   // MeshGeoUndo: vanimIdx
    if (g_pendingApplyXf) g_pendingApplyXf->RemapLista(lista, a, b);
    for (size_t i = 0; i < g_pendingApplyGeos.size(); i++) g_pendingApplyGeos[i]->RemapLista(lista, a, b);
    for (size_t i = 0; i < g_pendingApplyArms.size(); i++) g_pendingApplyArms[i]->RemapLista(lista, a, b);
    if (g_pendingKeys)  g_pendingKeys->RemapLista(lista, a, b);  // KeyframesUndo: la lista de curvas
    if (g_pendingBones) g_pendingBones->RemapLista(lista, a, b); // BonesUndo: tracks[].idx
    if (g_pendingPose)  g_pendingPose->RemapLista(lista, a, b);
    if (g_pendingPesos) g_pendingPesos->RemapLista(lista, a, b);
    if (g_pendingUV)    g_pendingUV->RemapLista(lista, a, b);
    if (g_pendingUVGeo) g_pendingUVGeo->RemapLista(lista, a, b); // MeshGeoUndo: vanimIdx
    if (g_pendingB2D)   g_pendingB2D->RemapLista(lista, a, b);   // Bones2DUndo: tracks2d[].idx
}
