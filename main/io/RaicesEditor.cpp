// ============================================================================
//  RaicesEditor.cpp — ver RaicesEditor.h. El editor sobre las raices del proyecto
//  (escenas 3D, juegos y prefabs): el cambio de raiz con sus precondiciones, el
//  selector, el estado base de un juego y los proveedores de las vistas "Scenes" /
//  "Prefabs" del outliner.
// ============================================================================
#include "io/RaicesEditor.h"
#include "W3dRaices.h"
#include "io/RecursosProyecto.h"
#include "io/MallasProyecto.h"           // W3dMallasTickEditor: publicar la malla que sale de edicion
#include "io/CambiosProyecto.h"         // el '*' de una raiz sin guardar (el selector)
#include "io/Prefabs.h"                  // los usuarios de un prefab (sus instancias) y su nombre
#include "io/PrefabsEditor.h"            // volver de editar un prefab regenera sus instancias
#include "objects/InstanciaPrefab.h"      // los usuarios de un prefab ("Select Users") son sus instancias
#include "objects/Objects.h"
#include "objects/Camera.h"              // la camara activa y su lente son parte del estado base de un juego
#include "objects/Light.h"
#include "objects/Mesh.h"                // los pesos de las shape keys (los posan los clips)
#include "objects/Armature.h"            // "Convert to Game": el clip y el cabezal que el cuadro le pone a un esqueleto
#include "script/W3dScript.h"            // "Convert to Game": los scripts de la copia, tal cual (W3dScriptDatos)
#include "animation/Animation.h"         // los clips de un juego: sus animaciones de escena
#include "animation/SkeletalAnimation.h" // g_animMix / W3dMixEscenasSoltar
#include "base/W3dInteractionState.h"    // InteractionMode / estado
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"        // salir de la local view
#include "ViewPorts/Outliner.h"          // soltar el arrastre / el modo mover del arbol que se deja
#include "ViewPorts/LayoutArbol.h"       // LayoutRaizCompleta
#include "ViewPorts/LayoutInput.h"       // LayoutModoElegir / ActualizarEditMeshActivo
#include "ViewPorts/Notificaciones.h"
#include "ViewPorts/PopUp/PopUpBase.h"   // PopUpActive: un popup abierto apunta a objetos de la raiz
#include "undo/Undo.h"
#include "config/W3dLang.h"
#include "render/OpcionesRender.h"       // g_redraw
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/draw/icons.h"
#include "base/W3dNombres.h"
#include "w3dlog.h"
#include <vector>
#include <map>
#include <cstdio>

extern bool SimActiva();
extern void W3dCurveEdicionCerrar();
extern void PropsOlvidarEscena();
extern bool PlayAnimation;

// (una ESCENA se anima y se renderiza: la camara; un JUEGO simula: el control de juego. Los dos son
// iconos que ya existian, no hay arte propio para las raices)
int W3dRaizIcono(int tipo) {
    return tipo == W3D_RAIZ_PREFAB ? (int)IconType::prefab
         : tipo == W3D_RAIZ_JUEGO  ? (int)IconType::gamepad : (int)IconType::camera;
}

// ============================================================================
//  EL ESTADO BASE DE UN JUEGO (su "frame 1")
//  Un juego no tiene animacion propia: lo que se ve en "Juego" es su estado inicial. Sus
//  animaciones de escena son CLIPS (los dispara Lua): elegir uno en el timeline lo muestra y
//  lo deja editar, y el scrub POSA a los objetos. Al volver a "Juego" (o al dejar la raiz) cada
//  objeto que los clips pueden posar recupera lo que tenia al salir de "Juego". La foto va por
//  SERIAL (nunca reciclado): un objeto borrado mientras tanto no se toca.
// ============================================================================
namespace {
struct BaseObj {
    Object* o; unsigned int serial;
    Vector3 pos, scale, rotEuler; Quaternion rot;
    bool visible, renderizable;
    float fov, nearClip, farClip;                 // camara
    float dif[4], amb[4], spe[4];                 // luz
    std::vector<float> formas;                    // pesos de las shape keys
};
}
static std::vector<BaseObj> gBase;
static Object* gBaseRaiz = NULL;         // de que raiz es la foto
static Camera* gBaseCamara = NULL;       // la camara activa (los cortes de un clip la cambian)
static unsigned int gBaseCamSerial = 0;

static void BaseJuntar(Object* o) {
    if (!o) return;
    if (o != SceneCollection) {
        BaseObj b;
        b.o = o; b.serial = o->serial;
        b.pos = o->pos; b.scale = o->scale; b.rotEuler = o->rotEuler; b.rot = o->Rot();
        b.visible = o->visible; b.renderizable = o->renderizable;
        b.fov = b.nearClip = b.farClip = 0.0f;
        for (int k = 0; k < 4; k++) b.dif[k] = b.amb[k] = b.spe[k] = 0.0f;
        if (o->getType() == ObjectType::camera) {
            Camera* c = (Camera*)o; b.fov = c->fov; b.nearClip = c->nearClip; b.farClip = c->farClip;
        } else if (o->getType() == ObjectType::light) {
            Light* l = (Light*)o;
            for (int k = 0; k < 4; k++) { b.dif[k] = l->diffuse[k]; b.amb[k] = l->ambient[k]; b.spe[k] = l->specular[k]; }
        } else if (o->getType() == ObjectType::mesh) {
            b.formas = ((Mesh*)o)->shapePesos;
        }
        gBase.push_back(b);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) BaseJuntar(o->Childrens[i]);
}
static void BasePoner(const BaseObj& b) {
    Object* o = b.o;
    o->pos = b.pos; o->scale = b.scale; o->SetRotSnapshot(b.rot, b.rotEuler);
    o->visible = b.visible; o->renderizable = b.renderizable;
    if (o->getType() == ObjectType::camera) {
        Camera* c = (Camera*)o; c->fov = b.fov; c->nearClip = b.nearClip; c->farClip = b.farClip;
    } else if (o->getType() == ObjectType::light) {
        Light* l = (Light*)o;
        for (int k = 0; k < 4; k++) { l->diffuse[k] = b.dif[k]; l->ambient[k] = b.amb[k]; l->specular[k] = b.spe[k]; }
    } else if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        for (size_t k = 0; k < b.formas.size(); k++) m->SetShapePeso((int)k, b.formas[k]);
    }
}
// el objeto sigue vivo y es el mismo? (su direccion pudo reciclarse: el serial no)
static bool BaseVivo(const BaseObj& b) {
    return SceneCollection && W3dRaizDe(b.o) == SceneCollection && b.o->serial == b.serial;
}
static const BaseObj* BaseDe(Object* o) {
    if (!o || gBaseRaiz != SceneCollection) return NULL;
    for (size_t i = 0; i < gBase.size(); i++)
        if (gBase[i].o == o && gBase[i].serial == o->serial) return &gBase[i];
    return NULL;
}
// los objetos que los CLIPS de la raiz activa pueden posar (sus animaciones de escena: la activa vive
// en AnimationObjects, las otras en su SceneAnimation)
static void ObjetosDeClips(std::vector<Object*>& out) {
    out.clear();
    for (size_t i = 0; i < AnimationObjects.size(); i++) if (AnimationObjects[i].obj) out.push_back(AnimationObjects[i].obj);
    for (size_t e = 0; e < SceneAnimations.size(); e++) {
        if ((int)e == SceneAnimActiva || !SceneAnimations[e]) continue;
        const std::vector<AnimationObject>& v = SceneAnimations[e]->objetos;
        for (size_t i = 0; i < v.size(); i++) if (v[i].obj) out.push_back(v[i].obj);
    }
}

void W3dJuegoBaseGuardar() {
    gBase.clear();
    gBaseRaiz = SceneCollection;
    gBaseCamara = CameraActive;
    gBaseCamSerial = CameraActive ? ((Object*)CameraActive)->serial : 0;
    BaseJuntar(SceneCollection);
}
bool W3dJuegoBaseHay() { return gBaseRaiz != NULL && gBaseRaiz == SceneCollection && !gBase.empty(); }
void W3dJuegoBaseOlvidar() { gBase.clear(); gBaseRaiz = NULL; gBaseCamara = NULL; gBaseCamSerial = 0; }
bool W3dJuegoBaseReponer() {
    if (!W3dJuegoBaseHay()) { W3dJuegoBaseOlvidar(); return false; }
    std::vector<Object*> clips;
    ObjetosDeClips(clips);
    int n = 0;
    for (size_t i = 0; i < clips.size(); i++) {
        const BaseObj* b = BaseDe(clips[i]);
        if (b && BaseVivo(*b)) { BasePoner(*b); n++; }
    }
    // los CORTES de camara de un clip cambian la camara activa: vuelve la del juego
    if (gBaseCamara && W3dRaizDe((Object*)gBaseCamara) == SceneCollection && ((Object*)gBaseCamara)->serial == gBaseCamSerial)
        CameraActive = gBaseCamara;
    w3dLogf("[raices] el juego vuelve a su estado base: %d objeto(s) de sus clips repuestos", n);
    W3dJuegoBaseOlvidar();
    W3dAnimCurvasInvalidar();   // (si despues se elige un clip en este mismo frame, tiene que posar)
    g_redraw = true;
    return true;
}
void W3dJuegoBaseAplicarA(Object* o) {
    const BaseObj* b = BaseDe(o);
    if (b && BaseVivo(*b)) BasePoner(*b);
}
// ~Object: una foto no puede quedar con un puntero a un objeto liberado (el serial ya lo cubre, pero
// ademas asi no crece con basura)
static void BaseDesvincular(Object* o) {
    for (size_t i = gBase.size(); i-- > 0; ) if (gBase[i].o == o) gBase.erase(gBase.begin() + (long)i);
    if ((Object*)gBaseCamara == o) gBaseCamara = NULL;
    if (gBaseRaiz == o) W3dJuegoBaseOlvidar();
}
namespace {
struct RegistrarBase { RegistrarBase() { W3dDesvincularRegistrar(BaseDesvincular); } } gRegistrarBase;
}

// ============================================================================
//  EL CAMBIO DE RAIZ
// ============================================================================
static void JuntarHojas(ViewportBase* n, std::vector<ViewportBase*>& out) {
    if (!n) return;
    if (n->isLeaf()) { out.push_back(n); return; }
    if (n->ContainerKind() == 1) { JuntarHojas(((ViewportRow*)n)->childA, out); JuntarHojas(((ViewportRow*)n)->childB, out); }
    else { JuntarHojas(((ViewportColumn*)n)->childA, out); JuntarHojas(((ViewportColumn*)n)->childB, out); }
}

// lo que la UI tiene de la raiz que se deja: la local view (su conjunto nombra objetos de alla), el
// modo mover y el arrastre del outliner, y el objeto que Properties tenia bindeado
static void SoltarUIDeLaRaiz() {
    std::vector<ViewportBase*> hojas;
    JuntarHojas(LayoutRaizCompleta(), hojas);
    for (size_t i = 0; i < hojas.size(); i++) {
        ViewportBase* v = hojas[i];
        if (v->ViewportKind() == 1) {
            Viewport3D* v3 = (Viewport3D*)v;
            if (v3->localViewActivo) v3->LocalViewSalir();
        } else if (VpEsOutliner(v)) {
            Outliner* o = (Outliner*)v;
            if (o->moviendo) o->MoverCancelar();
            o->dragObjeto = NULL; o->dragging = false; o->dropZona = -2; o->dropFila = -1;
            o->hoverFila = -1;
        }
    }
    PropsOlvidarEscena();
}
// la UI de la raiz nueva: el outliner arranca arriba y rearma su scroll ('conservarScroll': entre escenas abiertas,
// que el outliner muestra todas juntas, no se mueve)
static void RefrescarUI(bool conservarScroll = false) {
    std::vector<ViewportBase*> hojas;
    JuntarHojas(LayoutRaizCompleta(), hojas);
    for (size_t i = 0; i < hojas.size(); i++)
        if (VpEsOutliner(hojas[i])) {
            Outliner* o = (Outliner*)hojas[i];
            if (o->vista == W3D_VISTA_ESCENA && !conservarScroll) { o->PosY = 0; o->PosX = 0; }
            o->lastContentRows = -1;
            if (o->width > 0 && o->height > 0) o->Resize(o->width, o->height);
        }
    W3dRecursosVistaInvalidar();
    g_redraw = true;
}

// ============================================================================
//  LAS ESCENAS ABIERTAS A LA VEZ (W3dRaices.h): tildar en la lista abre; destildar cierra y libera la memoria
// ============================================================================
// cerrarla de verdad: fuera de la lista, sin sus pasos de undo ni su marca de "sin guardar", y fuera de la memoria
// (salvo la del proyecto -el bloque- y una que todavia no se guardo: quedan cargadas, solo se cierran)
static void DescargarCerrada(const std::string& nombre) {
    const int idx = W3dRaizBuscar(W3D_RAIZ_ESCENA, nombre);
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    if (idx < 0 || idx >= (int)fs.size()) return;
    Object* r = fs[(size_t)idx].raiz;
    W3dRaizSacarDeAbiertas(idx);
    if (r) { UndoOlvidarRaiz(r); W3dCambiosOlvidarRaiz(r); }
    std::vector<int> v(1, idx);
    if (r && W3dRaicesDescargar(v) == 0)
        w3dLogf("[raices] '%s' cerrada (queda en memoria: %s)", nombre.c_str(),
                fs[(size_t)idx].bloque ? "es la del bloque del proyecto" : "todavia no se guardo");
    else w3dLogf("[raices] '%s' cerrada y fuera de la memoria", nombre.c_str());
    RefrescarUI(true);
}
bool W3dRaizTildar(int idx, bool abrir, std::string* motivo) {
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    if (idx < 0 || idx >= (int)fs.size() || !W3dRaizEs3D(fs[(size_t)idx].tipo)) {
        if (motivo) *motivo = "It doesn't exist anymore";
        return false;
    }
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    if (abrir) {
        if (!W3dRaizAbrir(idx)) { if (motivo) *motivo = "It could not be loaded"; return false; }
        w3dLogf("[raices] '%s' abierta", fs[(size_t)idx].nombre.c_str());
        RefrescarUI(true);
        return true;
    }
    std::vector<int> ab;
    W3dRaicesAbiertas(ab);
    bool esta = false;
    for (size_t i = 0; i < ab.size(); i++) if (ab[i] == idx) esta = true;
    if (!esta) return true;
    if (ab.size() <= 1) { if (motivo) *motivo = "At least one scene stays open"; return false; }
    const std::string nombre = fs[(size_t)idx].nombre;
    // la ACTIVA no se cierra: primero pasa a ser activa la siguiente abierta
    if (idx == W3dRaizActiva()) {
        int otra = -1;
        for (size_t i = 0; i < ab.size() && otra < 0; i++) if (ab[i] != idx) otra = ab[i];
        if (!W3dActivarRaiz(otra, motivo)) return false;
    }
    // con cambios sin guardar: el cartel (guardar y cerrar / cerrar sin guardar / cancelar) lo termina
    if (W3dCambiosPreguntarCerrarEscena(nombre)) return true;
    DescargarCerrada(nombre);
    return true;
}
struct RaicesEngancheCerrar { RaicesEngancheCerrar() { W3dCambiosCerrarEscenaHook = DescargarCerrada; } };
static RaicesEngancheCerrar gRaicesEngancheCerrar;
// el objeto de la raiz de una escena abierta pasa a ser la activa (un click en el outliner o en el 3D sobre algo suyo)
bool W3dActivarRaizDe(Object* raiz) {
    if (!raiz || raiz == SceneCollection) return true;
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    for (size_t i = 0; i < fs.size(); i++)
        if (fs[i].raiz == raiz) {
            std::string motivo;
            if (W3dActivarRaiz((int)i, &motivo)) return true;
            if (!motivo.empty()) Notificar(T(motivo.c_str()), true);
            return false;
        }
    return false;
}

bool W3dActivarRaiz(int idx, std::string* motivo) {
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    if (idx < 0 || idx >= (int)fs.size()) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (idx == W3dRaizActiva()) return true;
    // ENTRE ESCENAS ABIERTAS (varias a la vez en el editor): el historial sigue (cada paso de undo sabe de que raiz es
    // y la activa antes de aplicarse: Undo.cpp) y el outliner no se mueve. Con un prefab de por medio (se edita
    // aislado) es como siempre: historial limpio
    // (la que no estaba abierta -el selector, "New Scene"- REEMPLAZA a la activa en la lista: la otra sale de la lista y
    //  queda en memoria, como siempre; ahi el historial se limpia)
    const int actVieja = W3dRaizActiva();
    const bool yaAbierta = W3dRaizEstaAbierta(idx);
    const bool viejaEs3D = actVieja >= 0 && actVieja < (int)fs.size() && W3dRaizEs3D(fs[(size_t)actVieja].tipo);
    const bool entreAbiertas = yaAbierta && W3dRaizEs3D(fs[(size_t)idx].tipo) && viejaEs3D;
    int posEnLista = -1;
    if (!yaAbierta && viejaEs3D) {
        std::vector<int> ab;
        W3dRaicesAbiertas(ab);
        for (size_t i = 0; i < ab.size(); i++) if (ab[i] == actVieja) posEnLista = (int)i;
    }
    // PRECONDICIONES: el Play tiene su snapshot de ESTA raiz (el Stop la restaura) y un transform en
    // curso tiene los objetos seleccionados en estadoObjetos
    if (SimActiva()) { if (motivo) *motivo = "Stop the game before changing the scene"; return false; }
    if (estado != editNavegacion) { if (motivo) *motivo = "Finish the current transform first"; return false; }
    // salir de lo que apunta a objetos de la raiz que se deja: un popup abierto, la edicion de una
    // curva, Edit/Pose/Paint (la malla que sale de edicion se PUBLICA en su recurso)
    if (PopUpActive) PopUpActive->Cerrar();
    // un JUEGO se deja SIEMPRE en su estado base ("Juego"): si se estaba editando uno de sus clips,
    // los objetos vuelven a su frame 1 (la foto de la base es de ESTA raiz), y una escena que se
    // volvio juego al ponerle el primer script queda en "Juego". Al volver, eso es lo que se ve.
    if (W3dJuegoBaseHay()) W3dJuegoBaseReponer();
    W3dJuegoBaseOlvidar();
    if (W3dRaizActivaEsJuego()) W3dRaizModoPorTipo(true);
    PlayAnimation = false;   // (reproducir la animacion de una no sigue en la otra)
    W3dCurveEdicionCerrar();
    if (InteractionMode != ObjectMode) LayoutModoElegir(ObjectMode);
    InteractionMode = ObjectMode;
    ActualizarEditMeshActivo();
    W3dMallasTickEditor();
    // el Mix de escenas movio objetos de ESTA raiz: vuelven a su base (sus capas quedan con ella)
    if (g_animMix) W3dMixEscenasSoltar();
    SoltarUIDeLaRaiz();
    // UNDO LIMPIO (si no es entre escenas abiertas): el historial nombra objetos de la raiz que se deja (validar contra
    // la otra convertiria sus pasos en no-ops o, peor, re-insertaria un borrado en la raiz equivocada)
    if (!entreAbiertas) {
        UndoLimpiar();
        // (sin historial ya no hay pasos que nombren una vista por su indice: las HUERFANAS -la vista de un clip
        //  que se borro- se pueden sacar de la lista)
        W3dClipsVistasPurgar();
    }
    const std::string antes = (W3dRaizActiva() >= 0) ? fs[(size_t)W3dRaizActiva()].nombre : std::string();
    const bool eraPrefab = (W3dRaizActiva() >= 0 && fs[(size_t)W3dRaizActiva()].tipo == W3D_RAIZ_PREFAB);
    if (!W3dRaizCambiarActiva(idx)) { if (motivo) *motivo = "It could not be loaded"; return false; }
    if (!entreAbiertas) W3dClipsVistasPurgar();
    if (posEnLista >= 0) W3dRaizSacarDeAbiertas(actVieja);
    W3dRaizAbrir(idx);   // (una escena que se activa queda abierta)
    if (posEnLista >= 0) W3dRaizMoverAbierta(idx, posEnLista);
    W3dMallasTickEditor();   // (la activa que se miraba era de la otra raiz: se suelta)
    // se DEJO DE EDITAR un prefab: sus instancias (de todas las raices cargadas) se regeneran con lo que quedo. Una
    // ESCENA tambien: se puede instanciar en otra ("escena:<nombre>", io/Prefabs.h)
    if (eraPrefab) W3dPrefabDejoDeEditarse(antes);
    else if (!antes.empty()) W3dPrefabDejoDeEditarse(W3dPrefabClaveEscena(antes));
    RefrescarUI(entreAbiertas);
    w3dLogf("[raices] editando %s '%s' (antes '%s')", W3dRaizTipoClave(W3dRaizTipoDe(idx)),
            fs[(size_t)idx].nombre.c_str(), antes.c_str());
    return true;
}

// ---- convertir escena <-> juego (con undo: el paso vuelve al tipo anterior) ----
struct UndoTipoRaiz { std::string nombre; int otro; };
static void UndoTipoRaizAplicar(void* d) {
    UndoTipoRaiz* u = (UndoTipoRaiz*)d;
    const int i = W3dRaizBuscar(W3D_RAIZ_ESCENA, u->nombre);
    if (i < 0) return;
    const int actual = W3dRaizTipoDe(i);
    if (SimActiva() || !W3dRaizFijarTipo(i, u->otro)) return;
    u->otro = actual;
    W3dRecursosVistaInvalidar();
    g_redraw = true;
}
static void UndoTipoRaizLiberar(void* d) { delete (UndoTipoRaiz*)d; }

bool W3dRaizConvertirActiva(int tipo, std::string* motivo) {
    const int act = W3dRaizActiva();
    if (act < 0) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    const int antes = W3dRaizTipoDe(act);
    if (!W3dRaizEs3D(antes) || !W3dRaizEs3D(tipo)) { if (motivo) *motivo = "A prefab can't be converted"; return false; }
    if (antes == tipo) return true;
    // un juego editando un clip vuelve antes a su base (lo que queda es su frame 1)
    if (W3dJuegoBaseHay()) W3dJuegoBaseReponer();
    W3dJuegoBaseOlvidar();
    PlayAnimation = false;
    if (!W3dRaizFijarTipo(act, tipo)) { if (motivo) *motivo = "It can't be converted"; return false; }
    UndoTipoRaiz* u = new UndoTipoRaiz();
    u->nombre = W3dRaices()[(size_t)act].nombre; u->otro = antes;
    UndoExterno f; f.aplicar = UndoTipoRaizAplicar; f.liberar = UndoTipoRaizLiberar;
    UndoPushExterno(f, u);
    W3dRecursosVistaInvalidar();
    w3dLogf("[raices] '%s' ahora es %s", u->nombre.c_str(), tipo == W3D_RAIZ_JUEGO ? "un juego" : "una escena");
    g_redraw = true;
    return true;
}

int W3dRaizCrearYAbrir(int tipo, const std::string& nombre) {
    const int idx = W3dRaizNueva(tipo, nombre.empty() ? std::string(W3dRaizNombrePorDefecto(tipo)) : nombre);
    std::string motivo;
    if (!W3dActivarRaiz(idx, &motivo)) {
        Notificar(std::string(T(motivo.c_str())), true);
        W3dRecursosVistaInvalidar();
        return -1;
    }
    return idx;
}

// ============================================================================
//  "CONVERT TO GAME": LA ESCENA SE DUPLICA COMO UN JUEGO NUEVO (ver RaicesEditor.h)
//  El juego arranca con lo que se ve en el cuadro actual y sin las animaciones de la escena. La copia sale del
//  lector de siempre: la raiz escrita EN SECO (io/GuardarW3D.cpp, sin el reposo del guardado: la pose del cuadro
//  actual es lo que se copia) y leida en la raiz nueva (importers/import_w3d.cpp). Nombres, scripts, fisica, hitbox,
//  constraints, instancias de prefab... viajan como en un guardado; las mallas, los materiales y las bibliotecas de
//  clips de jerarquia son los MISMOS recursos (van por su nombre). Las animaciones de escena (keyframes, cortes de
//  camara, tiras de esqueleto, capas del Mix) no se escriben: el juego nace con la "Scene" vacia de siempre.
//  Lo que el JSON en seco no lleva se toma del ORIGINAL (por el "_origen" de cada nodo):
//    - los clips de un esqueleto y las vertex anims de una malla: COMPARTIDOS, como entre las instancias de un prefab
//      (W3dPrefabArreglarHook: los mismos clips en memoria, los mismos frames con copia al escribir);
//    - los scripts tal cual (el orden de sus valores y cual esta elegido en el panel), como en Shift+D;
//    - lo que la animacion le puso en ESE cuadro y el archivo no guarda: los pesos de las formas de una malla y el
//      clip + cabezal de un esqueleto (el de su tira en la animacion de escena, o el clip que se edita en el
//      timeline: en "Juego" cada esqueleto muestra su clip activo desde su cabezal);
//    - la camara activa (la del corte de ese cuadro) y la coleccion activa.
// ============================================================================
extern bool W3dRaizSerializarSeco(Object* raiz, std::string& json);   // io/GuardarW3D.cpp
extern bool W3dRaizLeerCopiaSeca(const std::string& json,              // importers/import_w3d.cpp
                                 std::vector<std::pair<Object*, unsigned> >* origenes);
extern int g_undoSinRaiz;   // undo/Undo.cpp: el paso que se apila no es de la raiz activa

namespace {
// el clip y el cabezal que muestra un esqueleto en el cuadro actual (los casos de EvaluarPoseEsqueleto fuera de
// "Juego"). false = se ve en reposo, o lo posan sus capas del Mix (esas viajan con el: el juego tambien las usa)
bool ClipEnPantalla(Armature* a, int& clip, float& cabezal) {
    if (!a->capas.empty()) return false;
    int c = -1, f = CurrentFrame;
    if (ActiveAnimKind == 1 && ActiveAnimArm == a) c = a->animActiva;   // el clip que se edita en el timeline
    else if (ActiveAnimKind == 0 && SceneAnimActiva >= 0 && SceneAnimActiva < (int)SceneAnimations.size()) {
        // su TIRA en la animacion de escena activa: el frame 'frame' de la escena es el primero del clip
        const SceneAnimation* e = SceneAnimations[(size_t)SceneAnimActiva];
        if (e && !e->esClip)
            for (size_t t = 0; t < e->esqueletos.size(); t++) {
                if (e->esqueletos[t].armature != a->name) continue;
                c = W3dArmatureClipPorNombre(a, e->esqueletos[t].clip);
                if (c >= 0) f = a->animations[(size_t)c]->startFrame + (CurrentFrame - e->esqueletos[t].frame);
                break;
            }
    }
    if (c < 0 || c >= (int)a->animations.size() || !a->animations[(size_t)c]) return false;
    const SkeletalAnimation* cl = a->animations[(size_t)c];
    if (f > cl->endFrame) f = cl->endFrame;
    if (f < cl->startFrame) f = cl->startFrame;
    clip = c;
    cabezal = (float)(f - cl->startFrame);
    return true;
}
struct PoseEsqueleto { int clip; float cabezal; };
void JuntarPoses(Object* o, std::map<unsigned, PoseEsqueleto>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) {
        PoseEsqueleto p;
        if (ClipEnPantalla((Armature*)o, p.clip, p.cabezal)) out[o->serial] = p;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarPoses(o->Childrens[i], out);
}
void MapaSeriales(Object* o, std::map<unsigned, Object*>& out) {
    if (!o) return;
    out[o->serial] = o;
    for (size_t i = 0; i < o->Childrens.size(); i++) MapaSeriales(o->Childrens[i], out);
}
// lo que el JSON en seco no lleva, de 'o' (el original) a 'g' (su copia)
void CompletarCopia(Object* g, Object* o, const std::map<unsigned, PoseEsqueleto>& poses) {
    if (!g || !o || g->getType() != o->getType()) return;
    if (W3dPrefabArreglarHook) W3dPrefabArreglarHook(g, o);   // clips de esqueleto y vertex anims, compartidos
    if (o->scriptDatos) { delete g->scriptDatos; g->scriptDatos = new W3dScriptDatos(*o->scriptDatos); }
    if (o->getType() == ObjectType::mesh) {
        Mesh* gm = (Mesh*)g;
        const Mesh* om = (const Mesh*)o;
        for (size_t k = 0; k < om->shapePesos.size(); k++) gm->SetShapePeso((int)k, om->shapePesos[k]);
    } else if (o->getType() == ObjectType::armature) {
        std::map<unsigned, PoseEsqueleto>::const_iterator p = poses.find(o->serial);
        Armature* ga = (Armature*)g;
        if (p != poses.end() && p->second.clip < (int)ga->animations.size()) {
            ga->animActiva = p->second.clip;
            ga->juegoFrame = p->second.cabezal;
            ga->juegoTermino = false;
        }
    }
}
}

// el paso de undo de "Convert to Game": Ctrl+Z vuelve a la escena y SACA el juego (su arbol queda en el paso, como en
// un borrado de la vista Scenes); Ctrl+Y lo devuelve y lo vuelve a abrir. No es de ninguna raiz (g_undoSinRaiz): con
// el juego afuera el undo no podria activarlo antes de aplicar el paso, el paso cambia de raiz solo. (Pasar a una
// escena que no estaba abierta vacia el historial, como siempre: el paso sobrevive porque en ese momento no esta en
// ninguna pila -el undo lo saco para aplicarlo- y queda solo)
struct UndoAJuego { std::string escena, juego; bool fuera; W3dRaizSacada s; };
static void UndoAJuegoAplicar(void* d) {
    UndoAJuego* u = (UndoAJuego*)d;
    std::string motivo;
    if (!u->fuera) {
        const int ie = W3dRaizBuscar(W3D_RAIZ_ESCENA, u->escena);
        const int ij = W3dRaizBuscar(W3D_RAIZ_JUEGO, u->juego);
        if (ie < 0 || ij < 0 || ie == ij) return;
        if (W3dRaizActiva() == ij && !W3dActivarRaiz(ie, &motivo)) {
            if (!motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
            return;
        }
        // (activar la escena pudo correr los indices: se busca de nuevo)
        const int j = W3dRaizBuscar(W3D_RAIZ_JUEGO, u->juego);
        W3dRaizSacarDeAbiertas(j);
        if (!W3dRaizSacar(j, &u->s, &motivo)) {
            if (!motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
            return;
        }
        u->fuera = true;
    } else {
        Object* raiz = u->s.fila.raiz;
        if (!W3dRaizDevolver(u->s)) return;
        u->fuera = false;
        const int ij = W3dRaizDeObjeto(raiz);
        if (ij < 0) return;
        u->juego = W3dRaices()[(size_t)ij].nombre;   // (el nombre con el que quedo: el Ctrl+Z lo busca por el)
        if (!W3dActivarRaiz(ij, &motivo) && !motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
    }
    W3dRecursosVistaInvalidar();
    g_redraw = true;
}
static void UndoAJuegoLiberar(void* d) {
    UndoAJuego* u = (UndoAJuego*)d;
    if (u->fuera) W3dRaizSacadaLiberar(u->s);
    delete u;
}

int W3dRaizConvertirEnJuego(std::string* motivo) {
    const int act = W3dRaizActiva();
    if (act < 0 || act >= (int)W3dRaices().size() || !SceneCollection) { if (motivo) *motivo = "It doesn't exist anymore"; return -1; }
    if (W3dRaizTipoDe(act) != W3D_RAIZ_ESCENA) { if (motivo) *motivo = "It can't be converted"; return -1; }
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return -1; }
    if (estado != editNavegacion) { if (motivo) *motivo = "Finish the current transform first"; return -1; }
    // lo que esta a medio editar se cierra ANTES de la foto, como al dejar la raiz (W3dActivarRaiz): la malla que sale
    // de Edit Mode se publica en su recurso, la vista de un clip de jerarquia se escribe en su clip, y toda malla
    // tiene su recurso (la copia la nombra por el)
    PlayAnimation = false;
    W3dCurveEdicionCerrar();
    if (InteractionMode != ObjectMode) LayoutModoElegir(ObjectMode);
    InteractionMode = ObjectMode;
    ActualizarEditMeshActivo();
    W3dMallasTickEditor();
    W3dMallasAsegurarRecursos();
    W3dClipsVistasSincronizar();
    // LA FOTO DEL CUADRO ACTUAL (con la escena todavia activa: su animacion y su timeline son los globales)
    Object* original = SceneCollection;
    std::string json;
    if (!W3dRaizSerializarSeco(original, json)) { if (motivo) *motivo = "It can't be converted"; return -1; }
    std::map<unsigned, PoseEsqueleto> poses;
    JuntarPoses(original, poses);
    const unsigned camSerial = CameraActive ? ((Object*)CameraActive)->serial : 0u;
    const unsigned colSerial = (CollectionActive && CollectionActive != SceneCollection) ? CollectionActive->serial : 0u;
    const std::string escena = W3dRaices()[(size_t)act].nombre;
    const int cuadro = CurrentFrame;
    // EL JUEGO NUEVO, con el nombre de una copia de la escena ("Intro" -> "Intro.001": escenas y juegos comparten
    // nombres). Se arma como la carga de una raiz (W3dRaizCargar): mientras se lo llena es la activa
    const int idxNueva = W3dRaizNueva(W3D_RAIZ_JUEGO, escena);
    Object* juego = W3dRaices()[(size_t)idxNueva].raiz;
    const int antes = W3dRaizActiva();   // (la fila nueva pudo correr los indices)
    const unsigned selSerial = W3dSeleccionSerial;   // (armar la copia no es ELEGIR: ver W3dSeleccionSerial)
    W3dRaizUsar(idxNueva);
    std::vector<std::pair<Object*, unsigned> > origenes;
    const bool leyo = W3dRaizLeerCopiaSeca(json, &origenes);
    std::map<unsigned, Object*> orig;
    MapaSeriales(original, orig);
    for (size_t i = 0; i < origenes.size(); i++) {
        std::map<unsigned, Object*>::iterator t = orig.find(origenes[i].second);
        if (t == orig.end()) continue;
        Object* g = origenes[i].first;
        CompletarCopia(g, t->second, poses);
        if (camSerial && t->first == camSerial && g->getType() == ObjectType::camera) CameraActive = (Camera*)g;
        if (colSerial && t->first == colSerial) CollectionActive = g;
    }
    DeseleccionarTodo(); ObjActivo = NULL;   // (los constructores fueron seleccionando)
    W3dRaizUsar(antes);
    W3dSeleccionSerial = selSerial;
    if (!leyo) {
        W3dRaizBorrar(W3dRaizDeObjeto(juego), NULL);
        if (motivo) *motivo = "It can't be converted";
        return -1;
    }
    w3dLogf("[raices] la escena '%s' se duplico como el juego '%s' con su cuadro %d (%d objeto(s), sin sus animaciones "
            "de escena; la escena queda como estaba)", escena.c_str(), W3dRaices()[(size_t)W3dRaizDeObjeto(juego)].nombre.c_str(),
            cuadro, (int)origenes.size());
    // y pasa a ser la activa, como un "New Game" (la escena sale de la lista de abiertas y queda en memoria)
    if (!W3dActivarRaiz(W3dRaizDeObjeto(juego), motivo)) { W3dRecursosVistaInvalidar(); return -1; }
    // EL PASO DE UNDO, despues de activarlo (pasar a una raiz que no estaba abierta vacio el historial)
    UndoAJuego* u = new UndoAJuego();
    u->escena = escena;
    u->juego = W3dRaices()[(size_t)W3dRaizDeObjeto(juego)].nombre;
    u->fuera = false;
    UndoExterno f; f.aplicar = UndoAJuegoAplicar; f.liberar = UndoAJuegoLiberar;
    g_undoSinRaiz++;
    UndoPushExterno(f, u);
    g_undoSinRaiz--;
    W3dRecursosVistaInvalidar();
    g_redraw = true;
    return W3dRaizDeObjeto(juego);
}

// ============================================================================
//  EL SELECTOR
// ============================================================================
// los tres "New ..." (el selector y el "+" del outliner). 'iconoDeTipo': con el icono de lo que crean
// (el "+", cuyo titulo ya dice que se agrega algo) o con el "+" (el selector: ahi el icono de tipo los
// confundiria con las escenas/juegos/prefabs de la lista)
static void AgregarNuevas(PopupMenu* m, bool iconoDeTipo) {
    m->Agregar(T("New Scene"), W3D_RAIZ_MENU_NUEVA_ESCENA, iconoDeTipo ? W3dRaizIcono(W3D_RAIZ_ESCENA) : (int)IconType::mas);
    m->Agregar(T("New Game"), W3D_RAIZ_MENU_NUEVO_JUEGO, iconoDeTipo ? W3dRaizIcono(W3D_RAIZ_JUEGO) : (int)IconType::mas);
    m->Agregar(T("New Prefab"), W3D_RAIZ_MENU_NUEVO_PREFAB, iconoDeTipo ? W3dRaizIcono(W3D_RAIZ_PREFAB) : (int)IconType::mas);
}

void W3dRaicesMenuArmarNuevas(PopupMenu* m) {
    if (!m) return;
    m->Limpiar();
    AgregarNuevas(m, true);
}

void W3dRaicesMenuArmar(PopupMenu* m) {
    if (!m) return;
    m->Limpiar();
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    const int act = W3dRaizActiva();
    const int ini = W3dRaizInicialIdx();
    // escenas y juegos juntos (en el orden del proyecto: comparten nombres y el juego salta entre
    // ellos con cambiarEscena), despues los prefabs; cada uno con el icono de su tipo
    static const int kOrden[2] = { W3D_RAIZ_ESCENA, W3D_RAIZ_PREFAB };
    for (int k = 0; k < 2; k++)
        for (size_t i = 0; i < fs.size(); i++) {
            if (W3dRaizClase(fs[i].tipo) != kOrden[k]) continue;
            const int tipo = W3dRaizTipoDe((int)i);
            MenuItem* it = m->Agregar(fs[i].nombre, 1000 + (int)i, W3dRaizIcono(tipo));
            if (!it) continue;
            it->verde = ((int)i == act);
            // la escena/juego con la que ARRANCA el juego compilado
            if (W3dRaizEs3D(tipo) && (int)i == ini && fs.size() > 1) it->atajo = T("start");
        }
    AgregarNuevas(m, false);
    const int tAct = (act >= 0 && act < (int)fs.size()) ? W3dRaizTipoDe(act) : -1;
    // la escena/juego que se edita pasa a ser la que arranca el juego
    if (W3dRaizEs3D(tAct) && act != ini)
        m->Agregar(T("Set as Start Scene"), W3D_RAIZ_MENU_INICIAL, W3dRaizIcono(tAct));
    // una escena se DUPLICA como un juego nuevo (la foto de su cuadro actual, sin sus animaciones de escena: un juego
    // simula y no tiene keyframes propios); un juego vuelve a ser escena en el lugar
    if (tAct == W3D_RAIZ_ESCENA) m->Agregar(T("Convert to Game"), W3D_RAIZ_MENU_A_JUEGO, W3dRaizIcono(W3D_RAIZ_JUEGO));
    if (tAct == W3D_RAIZ_JUEGO)  m->Agregar(T("Convert to Scene"), W3D_RAIZ_MENU_A_ESCENA, W3dRaizIcono(W3D_RAIZ_ESCENA));
}

void W3dRaicesMenuAccion(int id) {
    std::string motivo;
    if (id == W3D_RAIZ_MENU_NUEVA_ESCENA) { W3dRaizCrearYAbrir(W3D_RAIZ_ESCENA, std::string()); return; }
    if (id == W3D_RAIZ_MENU_NUEVO_JUEGO)  { W3dRaizCrearYAbrir(W3D_RAIZ_JUEGO, std::string()); return; }
    if (id == W3D_RAIZ_MENU_NUEVO_PREFAB) { W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, std::string()); return; }
    if (id == W3D_RAIZ_MENU_A_JUEGO) {
        if (W3dRaizConvertirEnJuego(&motivo) < 0 && !motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
        return;
    }
    if (id == W3D_RAIZ_MENU_A_ESCENA) {
        if (!W3dRaizConvertirActiva(W3D_RAIZ_ESCENA, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
        return;
    }
    if (id == W3D_RAIZ_MENU_INICIAL) {
        const int act = W3dRaizActiva();
        if (act >= 0 && act < (int)W3dRaices().size()) W3dRaizFijarInicial(W3dRaices()[(size_t)act].nombre);
        g_redraw = true;
        return;
    }
    if (id >= 1000 && id < 1000 + (int)W3dRaices().size())
        if (!W3dActivarRaiz(id - 1000, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
}

bool W3dRaicesBotonSincronizar(Button* b) {
    if (!b) return false;
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    const int act = W3dRaizActiva();
    if (act < 0 || act >= (int)fs.size()) return false;
    const int icono = W3dRaizIcono(W3dRaizTipoDe(act));
    bool cambio = false;
    // (un '*' si la raiz tiene cambios sin guardar: io/CambiosProyecto.h)
    const std::string t = fs[(size_t)act].nombre + (W3dCambiosRaizSucia(act) ? "*" : "");
    if (b->text != t) { b->text = t; cambio = true; }
    if (b->icon != icono) { b->icon = icono; cambio = true; }
    if (cambio) g_redraw = true;
    return cambio;
}

// ============================================================================
//  LOS PROVEEDORES DE LAS VISTAS "Scenes" y "Prefabs" DEL OUTLINER
// ============================================================================
// el paso de UNDO de un renombre (ida y vuelta: aplicar intercambia 'actual' y 'otro')
struct UndoRenRaiz { int tipo; std::string actual, otro; };
// un PREFAB renombrado: sus instancias lo nombran por nombre. TODAS las raices se cargan antes (una que no se
// abrio lo seguiria nombrando con el viejo y al abrirla la instancia quedaria vacia), igual que al renombrar un
// recurso de la biblioteca
static void PrefabRenombrado(int tipo, const std::string& viejo, const std::string& nuevo) {
    if (tipo != W3D_RAIZ_PREFAB || viejo == nuevo) return;
    const int n = W3dPrefabRenombrado(viejo, nuevo);
    if (n) w3dLogf("[prefabs] %d instancia(s) siguen al prefab '%s' -> '%s'", n, viejo.c_str(), nuevo.c_str());
}
static void UndoRenRaizAplicar(void* d) {
    UndoRenRaiz* u = (UndoRenRaiz*)d;
    const int i = W3dRaizBuscar(u->tipo, u->actual);
    std::string quedo;
    if (u->tipo == W3D_RAIZ_PREFAB) W3dRaicesCargarTodas();
    if (i < 0 || !W3dRaizRenombrar(i, u->otro, &quedo)) return;
    PrefabRenombrado(u->tipo, u->actual, quedo);
    W3dRecursoActivoRenombrado(u->tipo == W3D_RAIZ_PREFAB ? W3D_VISTA_PREFABS : W3D_VISTA_ESCENAS, u->actual, quedo);
    u->otro = u->actual;
    u->actual = quedo;
    g_redraw = true;
}
static void UndoRenRaizLiberar(void* d) { delete (UndoRenRaiz*)d; }
// el paso de undo de un BORRADO (W3dRaizSacar): Ctrl+Z la devuelve con su arbol y su contexto, Ctrl+Y la vuelve a
// sacar. Mientras esta afuera su arbol es del paso (se libera si el paso se cae del historial).
struct UndoBorrarRaiz { int tipo; std::string nombre; bool fuera; W3dRaizSacada s; };
static void UndoBorrarRaizAplicar(void* d) {
    UndoBorrarRaiz* u = (UndoBorrarRaiz*)d;
    if (u->fuera) {
        Object* raiz = u->s.fila.raiz;
        if (!W3dRaizDevolver(u->s)) return;
        u->fuera = false;
        // el nombre con el que quedo (el Ctrl+Y la busca por el)
        const int i = W3dRaizDeObjeto(raiz);
        if (i >= 0) u->nombre = W3dRaices()[(size_t)i].nombre;
    } else {
        std::string motivo;
        if (!W3dRaizSacar(W3dRaizBuscar(u->tipo, u->nombre), &u->s, &motivo)) return;
        u->fuera = true;
    }
    W3dRecursosVistaInvalidar();
    g_redraw = true;
}
static void UndoBorrarRaizLiberar(void* d) {
    UndoBorrarRaiz* u = (UndoBorrarRaiz*)d;
    if (u->fuera) W3dRaizSacadaLiberar(u->s);
    delete u;
}

class ProveedorRaices : public W3dProveedorRecursos {
public:
    int tipo;
    explicit ProveedorRaices(int t) : tipo(t) {}
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        const std::vector<W3dRaizFila>& fs = W3dRaices();
        const int act = W3dRaizActiva();
        for (size_t i = 0; i < fs.size(); i++) {
            if (W3dRaizClase(fs[i].tipo) != tipo) continue;   // (la vista Scenes lista escenas Y juegos)
            const int t = W3dRaizTipoDe((int)i);
            W3dRecursoItem it;
            it.id = it.nombre = fs[i].nombre;
            it.carpeta = fs[i].carpeta;
            it.entrada = fs[i].entrada;
            it.icono = W3dRaizIcono(t);
            // los usuarios de un prefab son sus INSTANCIAS (en las raices cargadas); una escena no tiene
            it.usuarios = (t == W3D_RAIZ_PREFAB) ? W3dPrefabContarInstancias(fs[i].nombre) : 0;
            const int n = W3dRaizObjetos((int)i);
            char b[32]; snprintf(b, sizeof(b), "%d ", n);
            it.info = std::string(T(t == W3D_RAIZ_JUEGO ? "game" : t == W3D_RAIZ_PREFAB ? "prefab" : "scene")) + ", ";
            it.info += (n < 0) ? std::string(T("not loaded"))
                               : std::string(b) + T(n == 1 ? "object" : "objects");
            if ((int)i == act) it.info += std::string(", ") + T("active");
            if (W3dRaizEs3D(t) && (int)i == W3dRaizInicialIdx() && fs.size() > 1)
                it.info += std::string(", ") + T("start");
            // la CASILLA: abierta en el editor (cargada, a la vista y en el outliner). Los prefabs no (se editan solos)
            if (W3dRaizEs3D(t)) it.tilde = W3dRaizEstaAbierta((int)i) ? 1 : 0;
            it.cargado = (fs[i].raiz != NULL);
            out.push_back(it);
        }
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        return W3dRaizFijarCarpeta(W3dRaizBuscar(tipo, id), carpeta);
    }
    std::string IdPedido(const std::string& id, const std::string& nuevo) {
        (void)id; return W3dNombreNormalizar(nuevo, W3dRaizNombrePorDefecto(tipo));
    }
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        const int i = W3dRaizBuscar(tipo, id);
        std::string quedo;
        if (i >= 0 && tipo == W3D_RAIZ_PREFAB) W3dRaicesCargarTodas();   // (ver PrefabRenombrado)
        if (i < 0 || !W3dRaizRenombrar(i, nuevo, &quedo)) return false;
        PrefabRenombrado(tipo, id, quedo);
        if (final) *final = quedo;
        if (quedo != id) {
            UndoRenRaiz* u = new UndoRenRaiz();
            u->tipo = tipo; u->actual = quedo; u->otro = id;
            UndoExterno f; f.aplicar = UndoRenRaizAplicar; f.liberar = UndoRenRaizLiberar;
            UndoPushExterno(f, u);
            g_redraw = true;
        }
        return true;
    }
    // con Ctrl+Z (W3dRaizSacar: su arbol queda en el paso de undo)
    bool Borrar(const std::string& id, std::string* motivo) {
        if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
        UndoBorrarRaiz* u = new UndoBorrarRaiz();
        u->tipo = tipo; u->nombre = id; u->fuera = false;
        if (!W3dRaizSacar(W3dRaizBuscar(tipo, id), &u->s, motivo)) { delete u; return false; }
        u->fuera = true;
        if (tipo == W3D_RAIZ_PREFAB) W3dPrefabInvalidar(id);   // (sus instancias quedan con lo que ya generaron)
        UndoExterno f; f.aplicar = UndoBorrarRaizAplicar; f.liberar = UndoBorrarRaizLiberar;
        UndoPushExterno(f, u);
        W3dRecursosVistaInvalidar();
        g_redraw = true;
        return true;
    }
    // un PREFAB EN USO se borra IGUAL si el usuario lo confirma (el cartel dice cuantas instancias lo usan): sus
    // instancias quedan SIN el -conservan lo que ya generaron hasta que se regeneren; guardadas, abren vacias con
    // su cruz y sus overrides- y el Ctrl+Z lo devuelve. Es el mismo borrado (con su paso de undo)
    bool BorrarEnUso(const std::string& id, std::string* motivo) {
        if (tipo != W3D_RAIZ_PREFAB) { if (motivo) *motivo = "It is in use"; return false; }
        return Borrar(id, motivo);
    }
    bool SabeBorrarEnUso() const { return tipo == W3D_RAIZ_PREFAB; }
    // los USUARIOS de un prefab son sus instancias (las de las raices cargadas; "Select Users" elige las de la
    // raiz activa)
    void Usuarios(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        if (tipo != W3D_RAIZ_PREFAB) return;
        std::vector<InstanciaPrefab*> v;
        W3dPrefabInstancias(id, v);
        for (size_t i = 0; i < v.size(); i++) out.push_back((Object*)v[i]);
    }
    bool Purgable() const { return false; }
    // tildar = abrirla en el editor; destildar = cerrarla y sacarla de la memoria (ver W3dRaizTildar)
    bool Tildar(const std::string& id, bool on, std::string* motivo) {
        if (tipo != W3D_RAIZ_ESCENA) { if (motivo) *motivo = "It has no checkbox"; return false; }
        return W3dRaizTildar(W3dRaizBuscar(tipo, id), on, motivo);
    }
};

static ProveedorRaices gProvEscenas(W3D_RAIZ_ESCENA);
static ProveedorRaices gProvPrefabs(W3D_RAIZ_PREFAB);
namespace {
struct RegistrarProveedores {
    RegistrarProveedores() {
        W3dRecursosVistaRegistrar(W3D_VISTA_ESCENAS, &gProvEscenas);
        W3dRecursosVistaRegistrar(W3D_VISTA_PREFABS, &gProvPrefabs);
    }
} gRegistrarProveedores;
}
